{
  'DISABLE': True,
  'TOOLS': ['newlib', 'glibc',  'bionic', 'linux', 'win'],
  'SEARCH': [
    '../../../../third_party/zlib',
  ],
  'TARGETS': [
    {
      'NAME' : 'zlib',
      'TYPE' : 'lib',
      'INCLUDES': ['../../include/zlib'],      
      'SOURCES' : [
        'adler32.c',
        'compress.c',
        'contrib',
        'crc32.c',
        'deflate.c',
        'gzio.c',
        'infback.c',
        'inffast.c',
        'inflate.c',
        'inftrees.c',
        'trees.c',
        'uncompr.c',
        'zutil.c',
      ],
    }
  ],
  'HEADERS': [
    {
      'DEST': 'include/zlib',
      'FILES': [
        'crc32.h',
        'deflate.h',
        'inffast.h',
        'inffixed.h',
        'inflate.h',
        'inftrees.h',
        'mozzconf.h',
        'trees.h',
        'zconf.h',
        'zlib.h',
        'zutil.h',
      ],
    },
  ],
  'DATA': [
    'LICENSE',
    'google.patch',
    'mixed-source.patch',
    'README.chromium',
  ],
  'DEST': 'src',
  'NAME': 'zlib',
  'EXPERIMENTAL': True,
}

