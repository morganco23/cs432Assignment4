//
//  Display a rotating cube with lighting
//
//  Light and material properties are sent to the shader as uniform
//    variables.  Vertex positions and normals are attribute variables.
//  Shininess comes into play, so the side of a cube will not necessarily
//    appear as a uniform color.

#include "cs432.h"
#include "vec.h"
#include "mat.h"
#include "picking.h"
#include "matStack.h"
#include <string>

// tick: every 50 milliseconds
#define TICK_INTERVAL 50

// typedefs to make code more readable
typedef vec4  color4;
typedef vec4  point4;

// for now, we know that we only need 72 vertices for our cube
const int NumVertices = 72; //(6 faces)(2 triangles/face)(3 vertices/triangle)(2 cubes)

// the properties of our objects
static point4 points[NumVertices];
static vec3   normals[NumVertices];
static color4   colorsDiffuse[NumVertices];
static color4   colorsSpecular[NumVertices];
static color4   colorsAmbient[NumVertices];
static GLfloat  objShininess[NumVertices];

// defining some colors
static color4 RED(1.0,0.0,0.0,1.0);
static color4 GRAY(0.5,0.5,0.5,1.0);
static color4 BLUE(0,0,1.0,1.0);
static color4 GREEN(0.0,1.0,0.0,1.0);
static color4 YELLOW(1.0,1.0,0.0,1.0);
static color4 WHITE(1.0,1.0,1.0,1.0);
static color4 CYAN(0.0,1.0,1.0,1.0);
static color4 BLACK(0.0,0.0,0.0,1.0);

// Vertices of a unit cube centered at origin, sides aligned with axes
static point4 vertices[8] = {
    point4( -0.5, -0.5,  0.5, 1.0 ),
    point4( -0.5,  0.5,  0.5, 1.0 ),
    point4(  0.5,  0.5,  0.5, 1.0 ),
    point4(  0.5, -0.5,  0.5, 1.0 ),
    point4( -0.5, -0.5, -0.5, 1.0 ),
    point4( -0.5,  0.5, -0.5, 1.0 ),
    point4(  0.5,  0.5, -0.5, 1.0 ),
    point4(  0.5, -0.5, -0.5, 1.0 )
};

// Array of rotation angles (in degrees) for each coordinate axis;
// These are used in rotating the cube.
static int Axis = 0; // 0 => x-rotation, 1=>y, 2=>z, 3=>none
static GLfloat Theta[] = { 0.0, 0.0, 0.0, 0.0};
static float spinSpeed = 1;

// Model-view, model-view-start and projection matrices uniform location
static GLuint  ModelView, ModelViewStart, Projection;

// The matrix that defines where the camera is. This starts out +3 in the
// z-direction, but can change based on the user moving the camera with
// keyboard input
mat4 model_view_start = LookAt(0,0,10,0,0,0,0,1,0);

static MatrixStack mvstack;

// Variables used to control the moving of the light
static bool lightSpin = false; // whether the light is moving
static GLfloat lightAngle = 0.0; // the current lighting angle

// Variables used to control the moving of the dice
static float gravity = -.05;
static float resistance = 2;
static bool die1Moving = true;
static float die1Position[] = { 0,2,0 };

static mat4 rotmat = RotateX(0);
static float die1Velocity[] = { 0,0,0};
static float die1Rotation[] = { 0,0,0 };
static float die1AngularVelocity[] = { 10,-0.25,0 };
static int numBounces1 = 0;

// Variables used to control the game
static int player1Score = 0;
static int player2Score = 0;
static int playerNum;
static int runningSum = 0;

//----------------------------------------------------------------------------



// variable used in generating the vertices for our objects
static int Index = 0;

// quad generates a square (of our cube) using two triangles
//
// parameters:
// - a, b, c and d: the vertex-numbers for this square
// - col: the color of the square
//   - note: the ambient, specular and diffuse colors are all set to
//     be the same color
// - shininess: the shininess of the square
static void
quad( int a, int b, int c, int d, color4 col, GLfloat shininess)
{
    // Initialize temporary vectors along the quad's edge to
    //   compute its face normal 
    vec4 u = vertices[b] - vertices[a];
    vec4 v = vertices[c] - vertices[b];
	
    vec3 normal = normalize( cross(u, v) );
	
    // create the 6 faces, each with appropriate properties ...
    
    normals[Index] = normal; points[Index] = vertices[a]; colorsDiffuse[Index] = col;
    colorsSpecular[Index] = col; colorsAmbient[Index] = col; objShininess[Index] = shininess;
    Index++;
    
    normals[Index] = normal; points[Index] = vertices[b]; colorsDiffuse[Index] = col;
    colorsSpecular[Index] = col; colorsAmbient[Index] = col; objShininess[Index] = shininess;
    Index++;
    
    normals[Index] = normal; points[Index] = vertices[c]; colorsDiffuse[Index] = col;
    colorsSpecular[Index] = col; colorsAmbient[Index] = col; objShininess[Index] = shininess;
    Index++;
    
    normals[Index] = normal; points[Index] = vertices[a]; colorsDiffuse[Index] = col;
    colorsSpecular[Index] = col; colorsAmbient[Index] = col; objShininess[Index] = shininess;
    Index++;
    
    normals[Index] = normal; points[Index] = vertices[c]; colorsDiffuse[Index] = col;
    colorsSpecular[Index] = col; colorsAmbient[Index] = col; objShininess[Index] = shininess;
    Index++;
    
    normals[Index] = normal; points[Index] = vertices[d]; colorsDiffuse[Index] = col;
    colorsSpecular[Index] = col; colorsAmbient[Index] = col; objShininess[Index] = shininess;
    Index++;
}

//----------------------------------------------------------------------------

// creates a cug with each face having difference color/shininess properties
static void
colorcube()
{
    quad( 1, 0, 3, 2, RED, 1);
    quad( 2, 3, 7, 6, GRAY, 30);
    quad( 3, 0, 4, 7, GREEN, 300);
    quad( 6, 5, 1, 2, CYAN, 1);
    quad( 4, 5, 6, 7, YELLOW,30);
    quad( 5, 4, 0, 1, BLUE, 300);
}

//----------------------------------------------------------------------------

// the GPU light ID, this allows us to change the position of the light during execution
static int lightId;

// send updated light-position information to the GPU
static void updateLightPosition() {
    GLfloat lightX = sin(lightAngle*0.023);
    GLfloat lightY = sin(lightAngle*0.031);
    GLfloat lightZ = sin(lightAngle*0.037);
    vec4 pos(lightX,lightY,lightZ,0.0);
    glUniform4fv( lightId, 1, pos );
    
}
//----------------------------------------------------------------------------

// display the scene
static void
display( void )
{

    
    // set all to background color
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
    // compute the initial model-view matrix based on camera position
    mat4  model_view = model_view_start;
    

    //mvstack.push(model_view);

    // perform translations on the die
    model_view *= Translate(die1Position[0], die1Position[1], die1Position[2]);
    model_view *= RotateX(die1Rotation[0]);
    model_view *= RotateY(die1Rotation[1]);
    model_view *= RotateZ(die1Rotation[2]);
    
    
    
    // update the light position based on the light-rotation information
    updateLightPosition();
    
    // send uniform matrix variables to the GPU
    glUniformMatrix4fv( ModelViewStart, 1, GL_TRUE, model_view_start );
    glUniformMatrix4fv( ModelView, 1, GL_TRUE, model_view );

    // emit the cube to the scene
    setPickId(1); // set pick-id, in case we're picking
    glDrawArrays( GL_TRIANGLES, 0, NumVertices );
    clearPickId(); // clear pick-id
    
    
    //mvstack.pop();
    // swap buffers (so that just-drawn image is displayed) or perform picking,
    // depending on mode
    if (inPickingMode()) {
        endPicking();
    }
    else {
        glutSwapBuffers();
    }
}

// picking-finished callback: stop rotation if the cube has beenn selected
void scenePickingFcn(int code) {
    if (code == 1) { // the cube
        Axis = 3;
    }
}

//----------------------------------------------------------------------------

// for now, stop the spinning if the cube is clicked
static void
mouse( int button, int state, int x, int y ) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        // perform a "pick", including any associated action
        startPicking(scenePickingFcn, x, y);
    }

}

//----------------------------------------------------------------------------

// timer function, called when the timer has ticked
static void
tick(int n)
{
    glutTimerFunc(n, tick, n); // schedule next tick

    // change the appropriate axis based on spin-speed
    //Theta[Axis] += spinSpeed;
    
    float speedsquared = die1Velocity[0] * die1Velocity[0] + die1Velocity[1] *
        die1Velocity[1] + die1Velocity[2] * die1Velocity[2];

    std::cout << "X Position" << die1Position[0] << ", Y Position: " << die1Position[1] << std::endl;
    // update the position of the die by the different velocities
    if (die1Moving) {
        die1Velocity[1] += gravity;
        
        //we hit the ground. lets bounce
        if (die1Position[1] <=-1) {
            if (speedsquared < 0.02 || numBounces1 > 8) {
                die1Moving = false;
            }
            else
            {
                numBounces1++;
                die1Velocity[1] *= -1;
                die1Velocity[1] = die1Velocity[1] / (numBounces1);
                die1Position[1] = -1 + die1Velocity[1];
                die1Position[0] += die1Velocity[0] / (numBounces1);
                die1Position[2] += die1Velocity[2] / (numBounces1);
                // I'm thinking something like 60% of the rotation is converted to velocity in the direction of the y rotation
                
            }
            
        }
        else {
            die1Position[0] += die1Velocity[0] / (numBounces1 + 1);
            die1Position[1] += die1Velocity[1] / (numBounces1 + 1);
            die1Position[2] += die1Velocity[2] / (numBounces1 + 1);
            die1Rotation[0] += die1AngularVelocity[0];
            die1Rotation[1] += die1AngularVelocity[1];
            die1Rotation[2] += die1AngularVelocity[2];
            rotmat *= RotateX(die1AngularVelocity[0]);
            rotmat *= RotateY(die1AngularVelocity[1]);
            rotmat *= RotateZ(die1AngularVelocity[2]);
            die1AngularVelocity[0] /= resistance;
            die1AngularVelocity[1] /= resistance;
            die1AngularVelocity[2] /= resistance;
        }
        
    }

    // change the light angle
    if (lightSpin) {
        lightAngle += 5.0;
    }
    
    // tell GPU to display the frame
    glutPostRedisplay();
}
//----------------------------------------------------------------------------

// keyboard callback
static void
keyboard( unsigned char key, int x, int y )
{
    // Perform the appropriate action, based on the key that was pressed.
    // Default is to stop the cube-rotation
 	switch (key) {
        
		case 'q': case 'Q': case 033: // upper/lower Q or escape
            // Q: quit the program
			exit(0);
			break;
        case 'x': case 'X':
            // X: set rotation on X-axis
			Axis = 0;
			break;
		case 'y': case 'Y':
            // Y: set rotation on y-axis
			Axis = 1;
			break;
		case 'z': case 'Z':
            // Z: set rotation on z-axis
			Axis = 2;
			break;
        default:
            // default: stop spinning of cube
            Axis = 3;
            break;
		case '+': case '=':
            // + or =: increase spin-speed
			spinSpeed += 0.05;
			break;
		case '-': case '_':
            // - or _: decrease spin-speed
			spinSpeed -= 0.05;
			break;
        case 'l': case 'L':
            // L: toggle whether the light is spinning around scene
            lightSpin = !lightSpin;
            break;
        case 'w':
            // move forward
            model_view_start = Translate(0,0,0.1)*model_view_start;
            break;
        case 's':
            // move backward
            model_view_start = Translate(0,0,-0.1)*model_view_start;
            break;
        case 'a':
            // turn left
            model_view_start = RotateY(-1.5)*model_view_start;
            break;
        case 'd':
            // turn right
            model_view_start = RotateY(1.5)*model_view_start;
            break;
        case 'W':
            // turn up
            model_view_start = RotateX(-1.5)*model_view_start;
            break;
        case 'S':
            // turn down
            model_view_start = RotateX(1.5)*model_view_start;
            break;
        case 'A':
            // roll left
            model_view_start = RotateZ(-1.5)*model_view_start;
            break;
        case 'D':
            // roll right
            model_view_start = RotateZ(1.5)*model_view_start;
            break;
        case 'W'-64: // control-w
            // move up
            model_view_start = Translate(0,-0.1,0)*model_view_start;
            break;
        case 'S'-64: // control-s
            // move down
            model_view_start = Translate(0,0.1,0)*model_view_start;
            break;
        case 'A'-64: // control-a
            // move left
            model_view_start = Translate(0.1,0,0)*model_view_start;
            break;
        case 'D'-64: // control-d
            // move right
            model_view_start = Translate(-0.1,0,0)*model_view_start;
            break;
    }
}

//----------------------------------------------------------------------------

// window-reshape callback
void
reshape( int width, int height )
{
    glViewport( 0, 0, width, height );
	
    GLfloat aspect = GLfloat(width)/height;
    mat4  projection = Perspective( 65.0, aspect, 0.5, 100.0 );
	
    glUniformMatrix4fv( Projection, 1, GL_TRUE, projection );
}

// OpenGL initialization
static void
init()
{
    // create the cube object
    colorcube();
    
    // Create a vertex array object
    GLuint vao;
    glGenVertexArrays( 1, &vao );
    glBindVertexArray( vao );
    
    // Create and initialize a buffer object
    GLuint buffer;
    glGenBuffers( 1, &buffer );
    glBindBuffer( GL_ARRAY_BUFFER, buffer );
    glBufferData( GL_ARRAY_BUFFER,
                 sizeof(points) + sizeof(normals) + sizeof(colorsDiffuse) +
                 sizeof(colorsSpecular) + sizeof(colorsAmbient),
                 NULL, GL_STATIC_DRAW );
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(points), points );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(points),
                    sizeof(normals), normals );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(points) + sizeof(normals),
                    sizeof(colorsDiffuse), colorsDiffuse );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(points) + sizeof(normals) + sizeof(colorsDiffuse),
                    sizeof(colorsSpecular), colorsSpecular );
    glBufferSubData( GL_ARRAY_BUFFER,
                    sizeof(points) + sizeof(normals) + sizeof(colorsDiffuse) + sizeof(colorsSpecular),
                    sizeof(colorsAmbient), colorsAmbient );
    glBufferSubData( GL_ARRAY_BUFFER,
                    sizeof(points) + sizeof(normals) + sizeof(colorsDiffuse) + sizeof(colorsSpecular) + sizeof(colorsAmbient),
                    sizeof(objShininess), objShininess );
    
    // Load shaders and use the resulting shader program
    const GLchar* vShaderCode =
    // all of our attributes from the arrays uploaded to the GPU
    "attribute  vec4 vPosition; "
    "attribute  vec3 vNormal; "
    "attribute  vec4 vDiffCol; "
    "attribute  vec4 vSpecCol; "
    "attribute  vec4 vAmbCol; "
    "attribute  float vObjShininess; "
    
    // uniform variables
    "uniform mat4 ModelViewStart; "
    "uniform mat4 ModelView; "
    "uniform mat4 Projection; "
    "uniform vec4 LightPosition; "
    "uniform vec4 LightDiffuse; "
    "uniform vec4 LightSpecular; "
    "uniform vec4 LightAmbient; "
    "uniform vec4 PickColor; "
    
    // variables to send on to the fragment shader
    "varying vec3 N,L, E, H; "
    "varying vec4 colorAmbient, colorDiffuse, colorSpecular; "
    "varying float shininess; "
    
    // main vertex shader
    "void main() "
    "{ "
    
    // Transform vertex  position into eye coordinates
    "vec3 pos = (ModelView * vPosition).xyz; "
    " "
    // compute the lighting-vectors
    "N = normalize( ModelView*vec4(vNormal, 0.0) ).xyz; "
    "L = normalize( (ModelViewStart*LightPosition).xyz - pos ); "
    "E = normalize( -pos ); "
    "H = normalize( L + E ); "
    
    // pass on the material-related variables
    "colorAmbient = vAmbCol; "
    "colorDiffuse = vDiffCol; "
    "colorSpecular = vSpecCol; "
    "shininess = vObjShininess; "
    
    // convert the vertex to camera coordinates
    "gl_Position = Projection * ModelView * vPosition; "
    "} "
    ;
    const GLchar* fShaderCode =
    // variables passed from the vertex shader
    "varying vec3 N,L, E, H; "
    "varying float shininess; "
    "varying vec4 colorAmbient, colorDiffuse, colorSpecular; "
    
    // uniform variables
    "uniform vec4 light_ambient, light_diffuse, light_specular; "
    "uniform float Shininess; "
    "uniform vec4 PickColor; "
    
    // main fragment shader
    "void main()  "
    "{  "
    
    // if we are picking, use the pick color, ignoring everything else
    "if (PickColor.a >= 0.0) { "
    "   gl_FragColor = PickColor; "
    "   return;"
    "} "
    
    // compute color intensities
    "vec4 AmbientProduct = light_ambient * colorAmbient; "
    "vec4 DiffuseProduct = light_diffuse * colorDiffuse; "
    "vec4 SpecularProduct = light_specular * colorSpecular; "
    
    // Compute fragment colors based on illumination equations
    "vec4 ambient = AmbientProduct; "
    "float Kd = max( dot(L, N), 0.0 ); "
    "vec4  diffuse = Kd*DiffuseProduct; "
    "float Ks = pow( max(dot(N, H), 0.0), shininess ); "
    "vec4  specular = Ks * SpecularProduct; "
    "if( dot(L, N) < 0.0 ) { "
    "  specular = vec4(0.0, 0.0, 0.0, 1.0); } "
    
    // add the color components
    "  gl_FragColor = ambient + specular + diffuse; "
    "}  "
    ;
    
    // set up the GLSL shaders
    GLuint program = InitShader2(vShaderCode, fShaderCode);
    
    glUseProgram( program );
    
    // set up vertex arrays
    GLuint vPosition = glGetAttribLocation( program, "vPosition" );
    glEnableVertexAttribArray( vPosition );
    glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(0) );
    
    GLuint vNormal = glGetAttribLocation( program, "vNormal" );
    glEnableVertexAttribArray( vNormal );
    glVertexAttribPointer( vNormal, 3, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(sizeof(points)) );
    
    GLuint vDiffCol = glGetAttribLocation( program, "vDiffCol" );
    glEnableVertexAttribArray( vDiffCol );
    glVertexAttribPointer( vDiffCol, 4, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(sizeof(points)+sizeof(normals)) );
    
    GLuint vSpecCol = glGetAttribLocation( program, "vSpecCol" );
    glEnableVertexAttribArray( vSpecCol );
    glVertexAttribPointer( vSpecCol, 4, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(sizeof(points)+sizeof(normals)+sizeof(colorsDiffuse)) );
    
    GLuint vAmbCol = glGetAttribLocation( program, "vAmbCol" );
    glEnableVertexAttribArray( vAmbCol );
    glVertexAttribPointer( vAmbCol, 4, GL_FLOAT, GL_FALSE, 0,
                          BUFFER_OFFSET(sizeof(points)+sizeof(normals)+sizeof(colorsDiffuse)+sizeof(colorsSpecular)) );
    
    // Initialize lighting position and intensities
    point4 light_position( 1,1,1, 0 );
    color4 light_ambient( 0,0,0, 1.0 );
    color4 light_diffuse(1,1,1, 1.0);
    color4 light_specular( 0.4,0.4,0.4, 1.0 );
    
    glUniform4fv( glGetUniformLocation(program, "light_ambient"),
                 1, light_ambient );
    glUniform4fv( glGetUniformLocation(program, "light_diffuse"),
                 1, light_diffuse );
    glUniform4fv( glGetUniformLocation(program, "light_specular"),
                 1, light_specular );
    
    lightId = glGetUniformLocation(program, "LightPosition");
    
    // initialize picking
    setGpuPickColorId(glGetUniformLocation(program, "PickColor"));
    
    // Retrieve transformation uniform variable locations
    ModelViewStart = glGetUniformLocation( program, "ModelViewStart" );
    ModelView = glGetUniformLocation( program, "ModelView" );
    Projection = glGetUniformLocation( program, "Projection" );
    
    // enable z-buffer algorithm
    glEnable( GL_DEPTH_TEST );
    
    // set background color to be white
    glClearColor( 1.0, 1.0, 1.0, 1.0 );

    // initialize game variables
    playerNum = static_cast <int> (rand());
    float r1 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / .1));
    float r2 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / .1));
    float r3 = static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / .1));
    die1Velocity[0] = r1;
    die1Velocity[1] = r2;
    die1Velocity[2] = r3;



}

//----------------------------------------------------------------------------

int
main( int argc, char **argv )
{
    // perform OpenGL initialization
    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );
    glutInitWindowSize( 512, 512 );
    glutCreateWindow( "Pig" );
	glewInit();
    init();
	
    // set up callback functions
    glutDisplayFunc( display );
    glutKeyboardFunc( keyboard );
    glutReshapeFunc( reshape );
    glutMouseFunc( mouse );
    glutTimerFunc(TICK_INTERVAL, tick, TICK_INTERVAL); // timer callback

    // start executing the main loop, waiting for a callback to occur
    glutMainLoop();
    return 0;
}
