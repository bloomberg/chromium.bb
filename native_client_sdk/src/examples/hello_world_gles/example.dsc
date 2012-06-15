{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'hello_world',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.cc', 'matrix.cc', 'matrix.h'],
      'CXXFLAGS': [
        '$(NACL_CXXFLAGS)', 
        '-I../../src',
        '-I../../src/ppapi/lib/gl'
      ],
      'LDFLAGS': ['-g', '-pthread', '-lppapi', '-lppapi_gles2']
    }
  ],
  'DATA': [
    'fragment_shader_es2.frag',
    'hello.raw',
    'vertex_shader_es2.vert'
  ],
  'DEST': 'examples',
  'NAME': 'hello_world_gles',
}

