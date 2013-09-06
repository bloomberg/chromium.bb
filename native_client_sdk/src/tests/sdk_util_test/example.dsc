{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
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
  'NAME': 'sdk_util_test',
  'TITLE': 'SDK Util test',
}
