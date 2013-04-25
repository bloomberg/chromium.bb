{
  'DISABLE': True,
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'hello_world_instance3d',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.cc', 'matrix.cc', 'matrix.h'],
      'CXXFLAGS': [
        '-I../../src',
        '-I../../src/ppapi/lib/gl'
      ],
      'DEPS': ['ppapi_main', 'nacl_io'],
      'LIBS': ['ppapi_gles2', 'ppapi_cpp', 'ppapi',
               'pthread']
    }
  ],
  'DATA': [
    'fragment_shader_es2.frag',
    'hello.raw',
    'vertex_shader_es2.vert'
  ],
  'DEST': 'examples',
  'NAME': 'hello_world_instance3d',
  'TITLE': 'Hello World GLES 2.0 using ppapi_instance3d',
  'GROUP': 'API'
}



