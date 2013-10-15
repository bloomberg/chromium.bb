{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
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
      'LIBS': ['gmock', 'ppapi_cpp', 'ppapi', 'gtest', 'pthread'],
      'INCLUDES': ['$(NACL_SDK_ROOT)/include/gtest/internal'],
      'CXXFLAGS': ['-Wno-sign-compare', '-Wno-unused-private-field'],
      'CFLAGS_GCC': ['-Wno-unused-local-typedefs'],
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
