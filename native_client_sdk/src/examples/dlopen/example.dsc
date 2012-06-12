{
  'TOOLS': ['glibc'],
  'TARGETS': [
    {
      'NAME' : 'dlopen',
      'TYPE' : 'main',
      'SOURCES' : ['dlopen.cc'],
      'LDFLAGS' : ['-g','-ldl','-lppapi_cpp', '-lppapi']
    },
    {
      'NAME' : 'libeightball',
      'TYPE' : 'so',
      'SOURCES' : ['eightball.cc', 'eightball.h'],
      'CXXFLAGS': ['$(NACL_CXXFLAGS)', '-fPIC'],
      'LDFLAGS' : ['-g','-ldl','-lppapi_cpp', '-lppapi', '-shared']
    }
  ],
  'DEST': 'examples',
  'NAME': 'dlopen',
}

