{
  'TOOLS': ['newlib', 'glibc'],
  'SEARCH' : ['../../../../ppapi/lib/gl/gles2'],
  'TARGETS': [
    {
      'NAME' : 'libgles2',
      'TYPE' : 'lib',
      'SOURCES' : [
        'gl2ext_ppapi.c',
        'gl2ext_ppapi.h',
        'gles2.c'
      ],
    }
  ],
  'DEST': 'src',
  'NAME': 'gles2',
}

