{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'gamepad',
      'TYPE' : 'main',
      'SOURCES' : ['gamepad.cc', 'gamepad_module.cc', 'gamepad.h'],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DEST': 'examples',
  'NAME': 'gamepad',
  'TITLE': 'Gamepad Example.',
  'GROUP': 'API'
}

