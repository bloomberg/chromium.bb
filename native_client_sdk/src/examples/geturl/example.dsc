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
    'Makefile',
    'example.js',
    'geturl_success.html',
  ],
  'DEST': 'examples',
  'NAME': 'geturl',
  'TITLE': 'Get URL',
  'DESC': """
The Get URL example demonstrates fetching an URL and then displaying
its contents.  Clicking the GetURL button will cause a geturl_success.html
file to get loaded asynchronously, then displayed in a text box when the
load completes.""",
  'FOCUS': 'URL loading.',
  'GROUP': 'API'
}

