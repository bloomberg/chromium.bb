{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'mouselock',
      'TYPE' : 'main',
      'SOURCES' : ['mouselock.cc', 'mouselock.h'],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DEST': 'examples',
  'NAME': 'mouselock',
  'TITLE': 'Mouse Lock',
  'GROUP': 'Concepts'
}

