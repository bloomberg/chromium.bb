{
  # TODO(binji): pnacl fails with error:
  # In file included from gtest.cc:133:
  # pepper_23/include/gtest/internal/src/gtest-internal-inl.h:213:8:
  #   error: private field 'pretty_' is not used [-Werror,-Wunused-private-field]
  #
  # We can fix this by passing -Wno-unused-private-field to clang, but gcc
  # doesn't support this flag so we'll have to support splitting CXXFLAGS based
  # on TOOL first.
  'TOOLS': ['newlib:x86', 'newlib:x64', 'glibc', 'win', 'linux'],
  'SEARCH': [
    '.',
    '../../../../testing/gtest/include/gtest',
    '../../../../testing/gtest/include/gtest/internal',
    '../../../../testing/gtest/src',
  ],
  'TARGETS': [
    {
      'NAME' : 'gtest',
      'TYPE' : 'lib',
      'SOURCES' : [
        'gtest.cc',
        'gtest-death-test.cc',
        'gtest-filepath.cc',
        'gtest_main.cc',
        'gtest-port.cc',
        'gtest-printers.cc',
        'gtest-test-part.cc',
        'gtest-typed-test.cc',
        'nacl_gtest_dummy_sys.cc',
      ],
      # Ignore warnings:
      #   gtest.cc:2555: error: enumeration value ‘COLOR_DEFAULT’ not handled in switch
      #   gtest-typed-test.h:239:47: error: anonymous variadic macros were introduced in C99 [-Werror=variadic-macros]
      'CXXFLAGS': ['-Wno-switch-enum', '-Wno-variadic-macros'],
      'INCLUDES': [
        # See comment below about gtest-internal-inl.h
        '$(NACL_SDK_ROOT)/include/gtest/internal',
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        'gtest-death-test.h',
        'gtest.h',
        'gtest-message.h',
        'gtest-param-test.h',
        'gtest_pred_impl.h',
        'gtest-printers.h',
        'gtest_prod.h',
        'gtest-spi.h',
        'gtest-test-part.h',
        'gtest-typed-test.h',
      ],
      'DEST': 'include/gtest',
    },
    {
      'FILES': [
        'gtest-death-test-internal.h',
        'gtest-filepath.h',
        'gtest-internal.h',
        'gtest-linked_ptr.h',
        'gtest-param-util-generated.h',
        'gtest-param-util.h',
        'gtest-port.h',
        'gtest-string.h',
        'gtest-tuple.h',
        'gtest-type-util.h',
      ],
      'DEST': 'include/gtest/internal',
    },
    {
      # This is cheesy, but gtest.cc includes "src/gtest-internal-inl.h". Since
      # gtest is not installed in the SDK, I don't really care about the
      # directory layout.
      # TODO(binji): If we decide to include gtest, put this file in a better
      # spot.
      'FILES': [
        'gtest-internal-inl.h',
      ],
      'DEST': 'include/gtest/internal/src',
    },
  ],
  'DATA': [
    'Makefile'
  ],
  'DEST': 'testlibs',
  'NAME': 'gtest',
}
