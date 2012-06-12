{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'pong',
      'TYPE' : 'main',
      'SOURCES' : [
        'pong.cc',
        'pong.h',
        'pong_module.cc',
        'view.cc',
        'view.h',
      ],
    }
  ],
  'DEST': 'examples',
  'NAME': 'pong',
}

