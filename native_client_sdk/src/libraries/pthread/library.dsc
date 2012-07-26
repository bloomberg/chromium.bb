{
  'TOOLS': ['win'],
  'TARGETS': [
    {
      'NAME' : 'pthread',
      'TYPE' : 'lib',
      'SOURCES' : ['pthread.c'],
    }
  ],
  'HEADERS': [
    {
      'FILES': ['pthread.h'],
      'DEST': 'include/win',
    }
  ],
  'DEST': 'src',
  'NAME': 'pthread',
}

