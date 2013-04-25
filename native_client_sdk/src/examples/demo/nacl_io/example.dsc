{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],
  'TARGETS': [
    {
      'NAME' : 'nacl_io',
      'TYPE' : 'main',
      'SOURCES' : [
        'handlers.c',
        'handlers.h',
        'hello_nacl_io.c',
        'hello_nacl_io.h',
        'queue.c',
        'queue.h',
      ],
      'LIBS': ['ppapi', 'pthread', 'nacl_io']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'examples/demo',
  'NAME': 'nacl_io',
  'TITLE': 'Nacl IO Demo',
  'GROUP': 'Demo'
}
