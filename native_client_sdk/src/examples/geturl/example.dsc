{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'gamepad',
      'TYPE' : 'main',
      'SOURCES' : ['geturl.cc', 'geturl_handler.cc', 'geturl_handler.h'],
    }
  ],
  'DATA': ['geturl_success.html'],
  'DEST': 'examples',
  'NAME': 'geturl',
}

