{
  'TOOLS': ['glibc', 'clang-newlib', 'pnacl', 'linux', 'mac'],
  'SEL_LDR': True,
  'TARGETS': [
    {
      'NAME' : 'testing',
      'TYPE' : 'main',
      'SOURCES' : ['testing.cc'],
      'LIBS' : ['ppapi_simple_cpp', 'ppapi_cpp', 'ppapi', 'gtest', 'nacl_io', 'pthread'],
      'CXXFLAGS': ['-Wno-sign-compare']
    }
  ],
  'DATA': [
    'example.js'
  ],
  'DEST': 'examples/tutorial',
  'NAME': 'testing',
  'TITLE': 'Testing with gtest',
  'GROUP': 'Tutorial'
}

