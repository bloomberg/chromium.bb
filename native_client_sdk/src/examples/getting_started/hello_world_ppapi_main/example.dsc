{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'hello_world_stdio',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
      'LIBS': ['ppapi_main', 'nacl_io', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/getting_started',
  'NAME': 'hello_world_ppapi_main',
  'TITLE': 'Hello World (libppapi_main)',
  'GROUP': 'Getting Started'
}

