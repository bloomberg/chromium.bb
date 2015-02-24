{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'linux', 'mac', 'clang-newlib'],
  'SEL_LDR': True,

  'TARGETS': [
    {
      'NAME' : 'sdk_util_test',
      'TYPE' : 'main',
      'SOURCES' : [
        'main.cc',
        'string_util_test.cc',
      ],
      'DEPS': ['ppapi_simple', 'sdk_util', 'nacl_io'],
      # Order matters here: gtest has a "main" function that will be used if
      # referenced before ppapi.
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
