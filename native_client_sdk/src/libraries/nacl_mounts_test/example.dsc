{
  # TODO(binji): pnacl doesn't build right now because gtest doesn't build yet.
  'TOOLS': ['newlib', 'glibc', 'win', 'linux'],

  # Need to add ../../examples for common.js
  'SEARCH': ['.', '../../examples'],
  'TARGETS': [
    {
      'NAME' : 'nacl_mounts_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'kernel_intercept_test.cc',
        'kernel_object_test.cc',
        'kernel_proxy_test.cc',
        'mount_node_test.cc',
        'mount_test.cc',
        'path_test.cc',
      ],
      'LIBS': ['ppapi', 'pthread', 'gtest', 'nacl_mounts', 'ppapi_cpp', 'gtest_ppapi']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'tests',
  'NAME': 'nacl_mounts_test',
  'TITLE': 'NaCl Mounts test',
}
