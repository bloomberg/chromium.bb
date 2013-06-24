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
        'hashtable.c',
        'stringpool.c',
        'symtable.c',
        'xray.c'
      ],
      'CCFLAGS': [	
        '-DXRAY -O2'	
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
