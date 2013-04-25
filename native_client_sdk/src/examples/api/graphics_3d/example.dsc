{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'hello_world_gles',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.cc', 'matrix.cc', 'matrix.h'],
      'CXXFLAGS': [
        '-I../../src',
        '-I../../src/ppapi/lib/gl'
      ],
      'LIBS': ['ppapi_gles2', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'fragment_shader_es2.frag',
    'hello.raw',
    'vertex_shader_es2.vert'
  ],
  'DEST': 'examples/api',
  'NAME': 'graphics_3d',
  'TITLE': 'Graphics 3D',
  'GROUP': 'API'
}



