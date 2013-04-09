{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'websocket',
      'TYPE' : 'main',
      'SOURCES' : ['websocket.cc'],
      'LIBS': ['ppapi_cpp', 'ppapi']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples',
  'NAME': 'websocket',
  'TITLE': 'Websocket',
  'GROUP': 'API'
}
