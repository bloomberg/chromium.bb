{
  'TOOLS': ['newlib', 'glibc', 'bionic', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'audio',
      'TYPE' : 'main',
      'SOURCES' : ['audio.cc'],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/api',
  'NAME': 'audio',
  'TITLE': 'Audio',
  'GROUP': 'API',
}

