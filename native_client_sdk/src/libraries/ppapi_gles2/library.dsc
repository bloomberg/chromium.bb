{
  'TOOLS': ['win'],
  'SEARCH' : ['../../../../ppapi/lib/gl/gles2'],
  'TARGETS': [
    {
      'NAME' : 'ppapi_gles2',
      'TYPE' : 'lib',
      'SOURCES' : [
        'gl2ext_ppapi.c',
        'gl2ext_ppapi.h',
        'gles2.c'
      ],
    }
  ],
  'DEST': 'src',
  'NAME': 'ppapi_gles2',
}

