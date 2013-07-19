{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'SEL_LDR': True,
  'TARGETS': [
    {
      'NAME' : 'simple_hello_world',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
      'DEPS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp'],
      'LIBS': ['ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/getting_started',
  'NAME': 'simple_hello_world',
  'TITLE': 'Hello World (ppapi_simple)',
  'GROUP': 'Getting Started'
}

