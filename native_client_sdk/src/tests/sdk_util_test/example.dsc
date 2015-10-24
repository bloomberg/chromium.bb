{
  'TOOLS': ['clang-newlib', 'glibc', 'pnacl', 'linux', 'mac'],
  'SEL_LDR': True,

  'TARGETS': [
    {
      'NAME' : 'sdk_util_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'main.cc',
        'string_util_test.cc',
      ],
      'DEPS': ['ppapi_simple_cpp', 'sdk_util', 'nacl_io'],
      'LIBS': ['ppapi_simple_cpp', 'ppapi_cpp', 'gmock', 'nacl_io', 'ppapi', 'gtest', 'pthread'],
      'CXXFLAGS': ['-Wno-sign-compare']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'tests',
  'NAME': 'sdk_util_test',
  'TITLE': 'SDK Util test',
}
