{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],
  'TARGETS': [
    {
      'NAME' : 'hello_nacl_io',
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
  'DEST': 'examples',
  'NAME': 'hello_nacl_io',
  'TITLE': 'Hello, Nacl IO!',
  'DESC': """
The NaCl IO example demonstrates mapping standard FILE such as fopen,
fread, fwrite into mounts by linking in the nacl_io library.  This allows
developers to wrap Pepper API such as the File IO API or URL Loader into
standard blocking calls.""",
  'FOCUS': 'Using NaCl IO.',
  'GROUP': 'Concepts'
}
