{
  'TOOLS': ['newlib', 'glibc'],
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
    'dragger.js',
    'trackball.js',
    'tumbler.js',
    'vector3.js'
  ],
  'DEST': 'examples',
  'NAME': 'tumbler',
}

