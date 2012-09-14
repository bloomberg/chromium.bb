{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'input_events',
      'TYPE' : 'main',
      'SOURCES' : ['input_events.cc'],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': ['example.js'],
  'DEST': 'examples',
  'NAME': 'input_events',
  'TITLE': 'Input Events',
  'DESC': """
The Input Events example demonstrates how to handle events triggered by
the user. This example allows a user to interact with a square representing a
module instance. Events are displayed on the screen as the user clicks,
scrolls, types, inside or outside of the square.""",
  'INFO': 'Keyboard and mouse input, view change, and focus events.'
}

