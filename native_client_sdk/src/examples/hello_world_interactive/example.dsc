{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'hello_world',
      'TYPE' : 'main',
      'SOURCES' : [
        'hello_world.cc',
        'helper_functions.cc',
        'helper_functions.h'
      ]
    }
  ],
  'DEST': 'examples',
  'NAME': 'hello_world_interactive',
}

