{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'socket',
      'TYPE' : 'main',
      'SOURCES' : ['socket.cc'],
      'LIBS': ['ppapi_cpp', 'ppapi']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'PRE': '''\nCHROME_ARGS = --allow-nacl-socket-api=localhost\n''',
  'DEST': 'examples/api',
  'NAME': 'socket',
  'TITLE': 'socket',
  'GROUP': 'API',
  'SOCKET_PERMISSIONS': ["tcp-listen:*:*", "tcp-connect", "resolve-host", "udp-bind:*:*", "udp-send-to:*:*"]
}
