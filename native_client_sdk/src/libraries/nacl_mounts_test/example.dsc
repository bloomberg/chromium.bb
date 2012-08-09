{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],
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
        'test.cc',
      ],
      'LIBS': ['ppapi', 'pthread', 'gtest', 'nacl_mounts']
    }
  ],
  'DEST': 'testing',
  'NAME': 'nacl_mounts_test',
  'EXPERIMENTAL': True,
}
