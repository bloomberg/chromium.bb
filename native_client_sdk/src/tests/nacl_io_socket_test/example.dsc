{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'nacl_io_socket_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'main.cc',
        'socket_test.cc',
        'echo_server.cc',
        'echo_server.h',
      ],
      'DEPS': ['ppapi_simple', 'nacl_io'],
      # Order matters here: gtest has a "main" function that will be used if
      # referenced before ppapi.
      'LIBS': ['ppapi_simple', 'gmock', 'nacl_io', 'ppapi_cpp', 'ppapi', 'gtest', 'pthread'],
      'CXXFLAGS': ['-Wno-sign-compare']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'tests',
  'NAME': 'nacl_io_socket_test',
  'TITLE': 'NaCl IO Socket test',
  'SOCKET_PERMISSIONS': [
    "tcp-listen:*:*",
    "tcp-connect",
    "resolve-host",
    "udp-bind:*:*",
    "udp-send-to:*:*"
  ]
}
