{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'mouselock',
      'TYPE' : 'main',
      'SOURCES' : ['mouselock.cc', 'mouselock.h'],
    }
  ],
  'DATA': ['check_browser.js'],
  'DEST': 'examples',
  'NAME': 'mouselock'
}

