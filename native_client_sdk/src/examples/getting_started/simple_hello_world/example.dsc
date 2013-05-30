{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'simple_hello_world',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
      'LIBS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp', 'ppapi', 'pthread']
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

