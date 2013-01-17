{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'hello_world',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
      'LIBS': ['ppapi', 'pthread']
    }
  ],
  'DATA': [
    'Makefile',
    'example.js',
  ],
  'DEST': 'examples',
  'NAME': 'hello_world',
  'TITLE': 'Hello World.',
  'DESC': """
The Hello World In C example demonstrates the basic structure of all
Native Client applications. This example loads a Native Client module.  The
page tracks the status of the module as it load.  On a successful load, the
module will post a message containing the string "Hello World" back to
JavaScript which will display it as an alert.""",
  'FOCUS': 'Basic HTML, JavaScript, and module architecture.',
  'GROUP': 'Tools'
}
