{
  'TOOLS': ['glibc'],
  'TARGETS': [
    {
      'NAME': 'dlopen',
      'TYPE': 'main',
      'SOURCES': ['dlopen.cc'],
      'LIBS': ['nacl_io', 'dl', 'ppapi_cpp', 'ppapi', 'pthread']
    },
    {
      'NAME' : 'libeightball',
      'TYPE' : 'so',
      'SOURCES' : ['eightball.cc', 'eightball.h'],
      'CXXFLAGS': ['-fPIC'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    },
    {
      'NAME' : 'libreverse',
      # This .so file is manually loaded by dlopen; we don't want to include it
      # in the .nmf, or it will be automatically loaded on startup.
      'TYPE' : 'so-standalone',
      'SOURCES' : ['reverse.cc', 'reverse.h'],
      'CXXFLAGS': ['-fPIC'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/tutorial',
  'NAME': 'dlopen',
  'TITLE': 'Dynamic Library Open',
  'GROUP': 'Tutorial'
}

