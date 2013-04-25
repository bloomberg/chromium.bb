{
  'DISABLE': True,
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'hello_world_interactive',
      'TYPE' : 'main',
      'SOURCES' : [
        'hello_world.cc',
        'helper_functions.cc',
        'helper_functions.h'
      ],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples',
  'NAME': 'hello_world_interactive',
  'TITLE': 'Interactive Hello World in C++',
  'GROUP': 'Tools'
}

