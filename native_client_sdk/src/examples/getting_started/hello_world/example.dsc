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
    'example.js',
  ],
  'DEST': 'examples/getting_started',
  'NAME': 'hello_world',
  'TITLE': 'Hello World',
  'GROUP': 'Getting Started'
}
