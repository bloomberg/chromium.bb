{
  'TOOLS': ['newlib', 'glibc'],
  'SEARCH': [
    '.'
  ],
  'TARGETS': [
    {
      'NAME' : 'xray',
      'TYPE' : 'lib',
      'SOURCES' : [
        'demangle.c',
        'hashtable.c',
        'parsesymbols.c',
        'report.c',
        'stringpool.c',
        'symtable.c',
        'xray.c'
      ],
      'CCFLAGS': [
        '-DXRAY -DXRAY_ANNOTATE -O2'
      ]
    }
  ],
  'HEADERS': [
    {
      'FILES': [
        'xray.h',
        'xray_priv.h'
      ],
      'DEST': 'include/xray',
    }
  ],
  'DEST': 'src',
  'NAME': 'xray',
  'EXPERIMENTAL': True,
}
