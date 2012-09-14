{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
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
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': ['example.js'],
  'DEST': 'examples',
  'NAME': 'pong',
  'TITLE': 'Pong',
  'DESC': """
The Pong example demonstrates how to create a basic 2D video game and
how to store application information in a local persistent file. This game 
uses up and down arrow keyboard input events to move the paddle.""",
  'INFO': 'File I/O, 2D graphics, input events.'
}

