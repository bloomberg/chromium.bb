{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'life',
      'TYPE' : 'main',
      'SOURCES' : [
        'life.c',
      ],
      'DEPS': ['ppapi_simple', 'nacl_io'],
      'LIBS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DEST': 'examples/demo',
  'NAME': 'life',
  'TITLE': "Conway's Life",
  'GROUP': 'Demo'
}
