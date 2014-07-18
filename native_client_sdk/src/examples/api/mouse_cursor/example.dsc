{
  'TOOLS': ['newlib', 'glibc', 'bionic', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'mouse_cursor',
      'TYPE' : 'main',
      'SOURCES' : ['mouse_cursor.cc'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/api',
  'NAME': 'mouse_cursor',
  'TITLE': 'Mouse Cursor',
  'GROUP': 'API'
}

