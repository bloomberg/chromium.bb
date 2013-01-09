{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'SEARCH': [
    '.',
  ],
  'TARGETS': [
    {
      'NAME' : 'ppapi_main',
      'TYPE' : 'lib',
      'SOURCES' : [
        "ppapi_instance.cc",
        "ppapi_main.cc",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "ppapi_instance.h",
        "ppapi_main.h",
      ],
      'DEST': 'include/ppapi_main',
    },
  ],
  'DEST': 'src',
  'NAME': 'ppapi_main',
}
