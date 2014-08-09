{
  'TOOLS': ['bionic', 'newlib', 'glibc', 'pnacl', 'linux', 'win'],
  'SEARCH': [
    '../../../../testing/gmock/include/gmock',
    '../../../../testing/gmock/include/gmock/internal',
    '../../../../testing/gmock/src',
  ],
  'TARGETS': [
    {
      'NAME' : 'gmock',
      'TYPE' : 'lib',
      'SOURCES' : [
        'gmock.cc',
        'gmock-matchers.cc',
        'gmock-cardinalities.cc',
        'gmock-internal-utils.cc',
        'gmock-spec-builders.cc',
      ],
      #   gmock-spec-builders.cc:248: error: enumeration value ‘FAIL’ not handled in switch
      'CXXFLAGS': ['-Wno-switch-enum'],
      'DEPS': ['gtest'],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        'gmock-actions.h',
        'gmock-cardinalities.h',
        'gmock-generated-actions.h',
        'gmock-generated-actions.h.pump',
        'gmock-generated-function-mockers.h',
        'gmock-generated-function-mockers.h.pump',
        'gmock-generated-matchers.h',
        'gmock-generated-matchers.h.pump',
        'gmock-generated-nice-strict.h',
        'gmock-generated-nice-strict.h.pump',
        'gmock.h',
        'gmock-matchers.h',
        'gmock-more-actions.h',
        'gmock-more-matchers.h',
        'gmock-spec-builders.h',
      ],
      'DEST': 'include/gmock',
    },
    {
      'FILES': [
        'gmock-generated-internal-utils.h',
        'gmock-generated-internal-utils.h.pump',
        'gmock-internal-utils.h',
        'gmock-port.h',
      ],
      'DEST': 'include/gmock/internal',
    },
  ],
  'DEST': 'src',
  'NAME': 'gmock',
}
