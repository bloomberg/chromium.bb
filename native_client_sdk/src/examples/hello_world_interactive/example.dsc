{
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
  'DATA': ['example.js'],
  'DEST': 'examples',
  'NAME': 'hello_world_interactive',
  'TITLE': 'Interactive Hello World in C++',
  'DESC': """
The Interactive Hello World C++ example demonstrates the basic structure 
of all Native Client applications. This example loads a Native Client module
which uses two way interaction with JavaScript whenever a button is clicked.
The NaCl module will respond with the number 42 or the reversed version of the
string in the text box when the appropriate button is clicked.""",
  'INFO': 'Basic HTML, JavaScript, C++ PPAPI, and Messaging API.',
}

