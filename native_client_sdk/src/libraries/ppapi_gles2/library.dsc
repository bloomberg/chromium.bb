{
  'TOOLS': ['win', 'linux'],
  'SEARCH' : [
    '.',
    '../../../../ppapi/lib/gl/gles2'
  ],
  'TARGETS': [
    {
      'NAME' : 'ppapi_gles2',
      'TYPE' : 'lib',
      'SOURCES' : [
        'gl2ext_ppapi.c',
        'gles2.c'
      ],
    }
  ],
  'DATA': [
    'Makefile',
  ],
  'DEST': 'src',
  'NAME': 'ppapi_gles2',
}

