{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'file_histogram',
      'TYPE' : 'main',
      'SOURCES' : ['file_histogram.cc'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/api',
  'NAME': 'var_array_buffer',
  'TITLE': 'Var Array Buffer',
  'GROUP': 'API'
}

