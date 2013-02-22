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
  'DEST': 'examples',
  'NAME': 'hello_world_stdio',
  'TITLE': 'Hello World STDIO.',
  'DESC': """

  The Hello World Stdio example is the simplest one in the SDK.  It uses the
ppapi_main library which creates an Module and Instance, using default values
to simplify setup and communication with the PPAPI system.  In addition, it
uses the nacl_io library to remap IO to the Pepper API.  This
simplifies IO by providing a standard blocking API and allowing STDERR to go to
the JavaScript console by default.""",
  'FOCUS': 'Basic HTML, JavaScript, Minimal App.',
  'GROUP': 'Tools'
}

