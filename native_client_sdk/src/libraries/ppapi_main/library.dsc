{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'ppapi_main',
      'TYPE' : 'lib',
      'SOURCES' : [
        "ppapi_instance.cc",
        "ppapi_instance3d.cc",
        "ppapi_main.cc",
        "ppapi_queue.cc",
      ],
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        "ppapi_event.h",
        "ppapi_instance.h",
        "ppapi_instance3d.h",
        "ppapi_main.h",
        "ppapi_queue.h",
      ],
      'DEST': 'include/ppapi_main',
    },
  ],
  'DEST': 'src',
  'NAME': 'ppapi_main',
}
