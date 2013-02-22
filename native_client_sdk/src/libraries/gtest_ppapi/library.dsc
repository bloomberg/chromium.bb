{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'gtest_ppapi',
      # gtest-typed-test.h:239:47: error: anonymous variadic macros were introduced in C99 [-Werror=variadic-macros]
      'CXXFLAGS': ['-Wno-variadic-macros'],
      'TYPE' : 'lib',
      'SOURCES' : [
        "gtest_event_listener.cc",
        "gtest_instance.cc",
        "gtest_module.cc",
        "gtest_nacl_environment.cc",
        "gtest_runner.cc",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "gtest_event_listener.h",
        "gtest_instance.h",
        "gtest_module.h",
        "gtest_nacl_environment.h",
        "gtest_runner.h",
        "thread_condition.h",
      ],
      'DEST': 'include/gtest_ppapi',
    },
  ],
  'DEST': 'testlibs',
  'NAME': 'gtest_ppapi',
}
