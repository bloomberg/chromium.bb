{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],
  'TARGETS': [
    {
      'NAME' : 'nacl_io',
      'TYPE' : 'main',
      'SOURCES' : [
        'handlers.c',
        'handlers.h',
        'nacl_io_demo.c',
        'nacl_io_demo.h',
        'queue.c',
        'queue.h',
      ],
      'LIBS': ['nacl_io', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'examples/demo',
  'NAME': 'nacl_io',
  'TITLE': 'Nacl IO Demo',
  'GROUP': 'Demo',
  'PERMISSIONS': [
    'unlimitedStorage'
  ]
}
