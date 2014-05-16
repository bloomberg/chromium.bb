{
  'TOOLS': ['pnacl'],
  'TARGETS': [
    {
      'NAME' : 'life_simd',
      'TYPE' : 'main',
      'SOURCES' : [
        'life.c',
      ],
      'DEPS': ['ppapi_simple', 'nacl_io'],
      'LIBS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DEST': 'examples/demo',
  'NAME': 'life_simd',
  'TITLE': "Conway's Life (SIMD version)",
  'GROUP': 'Demo'
}
