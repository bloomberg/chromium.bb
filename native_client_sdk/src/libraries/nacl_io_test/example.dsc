{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'SEL_LDR': True,

  # Need to add ../../examples for common.js
  'SEARCH': ['.', '../../examples'],
  'TARGETS': [
    {
      'NAME' : 'nacl_io_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'event_test.cc',
        'kernel_object_test.cc',
        'kernel_proxy_mock.cc',
        'kernel_proxy_mock.h',
        'kernel_proxy_test.cc',
        'kernel_wrap_test.cc',
        'main.cc',
        'mock_util.h',
        'mount_html5fs_test.cc',
        'mount_http_test.cc',
        'mount_mock.cc',
        'mount_mock.h',
        'mount_node_mock.cc',
        'mount_node_mock.h',
        'mount_node_test.cc',
        'mount_test.cc',
        'path_test.cc',
        'pepper_interface_mock.cc',
        'pepper_interface_mock.h',
	'socket_test.cc',
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
  'NAME': 'nacl_io_test',
  'TITLE': 'NaCl IO test',
}
