{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'geturl',
      'TYPE' : 'main',
      'SOURCES' : ['geturl.cc', 'geturl_handler.cc', 'geturl_handler.h'],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
    'geturl_success.html',
  ],
  'DEST': 'examples/api',
  'NAME': 'url_loader',
  'TITLE': 'URL Loader',
  'GROUP': 'API'
}

