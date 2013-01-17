{
  'TOOLS': ['newlib', 'glibc', 'win'],
  'TARGETS': [
    {
      'NAME' : 'hello_nacl_mounts',
      'TYPE' : 'main',
      'SOURCES' : [
        'handlers.c',
        'handlers.h',
        'hello_nacl_mounts.c',
        'hello_nacl_mounts.h',
        'queue.c',
        'queue.h',
      ],
      'LIBS': ['ppapi', 'pthread', 'nacl_mounts']
    }
  ],
  'DATA': [
    'Makefile',
    'example.js'
  ],
  'DEST': 'examples',
  'NAME': 'hello_nacl_mounts',
  'TITLE': 'Hello, Nacl Mounts!',
  'DESC': """
The NaCl Mounts example demonstrates mapping standard FILE such as fopen,
fread, fwrite into mounts by linking in the nacl_mounts library.  This allows
developers to wrap Pepper API such as the File IO API or URL Loader into
standard blocking calls.""",
  'FOCUS': 'Using NaCl Mounts.',
  'GROUP': 'Concepts'
}
