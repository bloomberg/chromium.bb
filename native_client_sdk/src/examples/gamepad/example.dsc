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
  'DATA': [
    'Makefile',
  ],
  'DEST': 'examples',
  'NAME': 'gamepad',
  'TITLE': 'Gamepad Example.',
  'DESC': """
Attached gamepad values should appear, left to right, once they've been
interacted with. Buttons, esp triggers are analog.
""",
  'FOCUS': 'Gamepad interface.',
  'GROUP': 'API'
}

