{
  'TOOLS': ['newlib', 'glibc', 'win'],
  'TARGETS': [
    {
      'NAME' : 'hello_nacl_mounts',
      'TYPE' : 'main',
      'SOURCES' : ['hello_nacl_mounts.c'],
      'LIBS': ['ppapi', 'pthread', 'nacl_mounts']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'examples',
  'NAME': 'hello_nacl_mounts',
  'TITLE': 'Hello, Nacl Mounts!',
}
