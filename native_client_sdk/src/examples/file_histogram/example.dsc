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
  'DEST': 'examples',
  'NAME': 'file_histogram',
  'TITLE': 'File Histogram.',
  'GROUP': 'API'
}

