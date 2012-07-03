{
  'TOOLS': ['newlib'],
  'TARGETS': [
    {
      'NAME' : 'tumbler',
      'TYPE' : 'main',
      'SOURCES' : [
        'callback.h',
        'cube.cc',
        'cube.h',
        'opengl_context.cc',
        'opengl_context.h',
        'opengl_context_ptrs.h',
        'scripting_bridge.cc',
        'scripting_bridge.h',
        'shader_util.cc',
        'shader_util.h',
        'transforms.cc',
        'transforms.h',
        'tumbler.cc',
        'tumbler.h',
        'tumbler_module.cc'
      ],
      'LIBS': ['ppapi_cpp', 'ppapi_gles2']
    }
  ],
  'DATA': [
    'bind.js',
    'dragger.js',
    'trackball.js',
    'tumbler.js',
    'vector3.js'
  ],
  'DEST': 'examples',
  'NAME': 'tumbler',
  'TITLE': 'Tumbler',
  'DESC': """
The Tumbler example demonstrates how to create a 3D cube that you can
rotate with your mouse while pressing the left mouse button. This example
creates a 3D context and draws to it using OpenGL ES. The JavaScript
implements a virtual trackball interface to map mouse movements into 3D
rotations using simple 3D vector math and quaternions.""",
  'INFO': '3D graphics'
}

