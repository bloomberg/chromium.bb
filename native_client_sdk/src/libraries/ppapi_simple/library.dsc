{
  'TOOLS': ['bionic', 'newlib', 'glibc', 'pnacl', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'ppapi_simple',
      'TYPE' : 'lib',
      'SOURCES' : [
        "ps.cc",
        "ps_context_2d.cc",
        "ps_event.cc",
        "ps_instance.cc",
        "ps_interface.cc",
        "ps_main.cc",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "ps.h",
        "ps_context_2d.h",
        "ps_event.h",
        "ps_instance.h",
        "ps_interface.h",
        "ps_main.h",
      ],
      'DEST': 'include/ppapi_simple',
    },
  ],
  'DEST': 'src',
  'NAME': 'ppapi_simple',
}
