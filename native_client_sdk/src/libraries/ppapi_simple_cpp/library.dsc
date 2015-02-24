{
  'TOOLS': ['bionic', 'newlib', 'glibc', 'pnacl', 'linux', 'mac', 'clang-newlib'],
  'SEARCH': [
    '.',
    '../ppapi_simple'
  ],
  'TARGETS': [
    {
      'NAME' : 'ppapi_simple_cpp',
      'TYPE' : 'lib',
      'SOURCES' : [
        "ps.c",
        "ps_context_2d.c",
        "ps_event.c",
        "ps_instance.c",
        "ps_interface.c",
        "ps_main.c",
        "ps_entrypoints_cpp.cc"
      ],
    },
  ],
  'DEST': 'src',
  'NAME': 'ppapi_simple_cpp',
}
