{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
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
      'DEPS': ['nacl_io'],
      'LIBS': ['ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'examples/demo',
  'NAME': 'nacl_io',
  'TITLE': 'NaCl IO Demo',
  'GROUP': 'Demo',
  'PERMISSIONS': [
    'unlimitedStorage'
  ],
  'SOCKET_PERMISSIONS': [
    'resolve-host'
  ]
}
