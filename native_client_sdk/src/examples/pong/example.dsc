{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'pong',
      'TYPE' : 'main',
      'SOURCES' : [
        'pong_instance.cc',
        'pong_instance.h',
        'pong_input.cc',
        'pong_input.h',
        'pong_model.cc',
        'pong_model.h',
        'pong_module.cc',
        'pong_view.cc',
        'pong_view.h'
      ],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': ['example.js'],
  'DEST': 'examples',
  'NAME': 'pong',
  'TITLE': 'Pong',
  'DESC': """
The Pong example demonstrates how to create a basic 2D video game. This game 
uses up and down arrow keyboard input events to move the paddle.""",
  'INFO': '2D graphics, input events.'
}

