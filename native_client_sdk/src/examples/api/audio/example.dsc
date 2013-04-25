{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'sine_synth',
      'TYPE' : 'main',
      'SOURCES' : ['sine_synth.cc'],
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

