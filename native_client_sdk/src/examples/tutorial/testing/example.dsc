{
  'TOOLS': ['glibc', 'newlib', 'pnacl', 'win'],
  'SEL_LDR': True,
  'TARGETS': [
    {
      'NAME' : 'testing',
      'TYPE' : 'main',
      'SOURCES' : ['testing.cc'],
      'LIBS' : ['ppapi_simple', 'nacl_io', 'ppapi_cpp', 'ppapi', 'gtest', 'pthread']
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

