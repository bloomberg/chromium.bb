{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'linux'],
  'SEL_LDR': True,
  'TARGETS': [
    {
      'NAME' : 'using_ppapi_simple',
      'TYPE' : 'main',
      'SOURCES' : ['hello_world.c'],
      'DEPS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp'],
      'LIBS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/tutorial',
  'NAME': 'using_ppapi_simple',
  'TITLE': 'Using the ppapi_simple library',
  'GROUP': 'Tutorial'
}
