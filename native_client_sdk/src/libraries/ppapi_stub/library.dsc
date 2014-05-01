{
  'TOOLS': ['bionic'],
  'SEARCH': [
      '.',
  ],
  'TARGETS': [
    {
      'NAME' : 'ppapi_stub',
      'TYPE' : 'lib',
      'SOURCES' : [
        'main.c',
        'ppapi_main.c',
      ],
    }
  ],
  'DEST': 'src',
  'NAME': 'ppapi_stub',
}

