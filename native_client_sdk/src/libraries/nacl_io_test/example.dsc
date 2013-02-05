{
  # TODO(binji): pnacl doesn't build right now because gtest doesn't build yet.
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],

  # Need to add ../../examples for common.js
  'SEARCH': ['.', '../../examples'],
  'TARGETS': [
    {
      'NAME' : 'nacl_io_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'kernel_object_test.cc',
        'kernel_proxy_mock.cc',
        'kernel_proxy_mock.h',
        'kernel_proxy_test.cc',
        'kernel_wrap_test.cc',
        'module.cc',
        'mount_node_test.cc',
        'mount_html5fs_test.cc',
        'mount_http_test.cc',
        'mount_test.cc',
        'path_test.cc',
        'pepper_interface_mock.cc',
        'pepper_interface_mock.h',
      ],
      'LIBS': ['ppapi', 'pthread', 'gtest', 'gmock', 'nacl_io', 'ppapi_cpp', 'gtest_ppapi']
    }
  ],
  'DATA': [
    'Makefile',
    'example.js'
  ],
  'DEST': 'tests',
  'NAME': 'nacl_io_test',
  'TITLE': 'NaCl IO test',
}
