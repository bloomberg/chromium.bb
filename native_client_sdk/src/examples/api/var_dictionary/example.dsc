{
  'TOOLS': ['newlib', 'glibc', 'bionic', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'var_dictionary',
      'TYPE' : 'main',
      'SOURCES' : ['var_dictionary.cc'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/api',
  'NAME': 'var_dictionary',
  'TITLE': 'Var Dictionary',
  'GROUP': 'API'
}

