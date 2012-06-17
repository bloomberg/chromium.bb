{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
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
      'LDFLAGS': ['$(NACL_LDFLAGS)', '-lppapi_gles2']
    }
  ],
  'DATA': [
    'bind.js',
    'check_browser.js',
    'dragger.js',
    'trackball.js',
    'tumbler.js',
    'vector3.js'
  ],
  'DEST': 'examples',
  'NAME': 'fullscreen_tumbler',
  'TITLE': 'Interactive Cube Example',
  'DESC': """
This is a modified version of the Tumbler example above that supports
full-screen display. It is in every way identical to Tumbler in
functionality, except that it adds the ability to switch to/from
full-screen display by pressing the Enter key.
""",
  'INFO': 'Teaching focus: Full-screen.'
}

