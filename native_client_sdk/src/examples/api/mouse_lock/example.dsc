{
  'DISABLE_PACKAGE': True,
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'mouselock',
      'TYPE' : 'main',
      'SOURCES' : ['mouselock.cc', 'mouselock.h'],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DEST': 'examples/api',
  'NAME': 'mouse_lock',
  'TITLE': 'Mouse Lock',
  'GROUP': 'API'
}

