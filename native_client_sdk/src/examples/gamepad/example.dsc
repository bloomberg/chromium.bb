{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'gamepad',
      'TYPE' : 'main',
      'SOURCES' : ['gamepad.cc', 'gamepad_module.cc', 'gamepad.h'],
      'LIBS': ['ppapi_cpp']
    }
  ],
  'DEST': 'examples',
  'NAME': 'gamepad',
  'TITLE': 'Gamepad Example.',
  'DESC': """
Attached gamepad values should appear, left to right, once they've been
interacted with. Buttons, esp triggers are analog.
""",
  'INFO':  'Gamepad interface.'
}

