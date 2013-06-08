{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'voronoi',
      'TYPE' : 'main',
      'SOURCES' : [
        'voronoi.cc'
      ],

      'LIBS': ['sdk_util', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/demo',
  'NAME': 'voronoi',
  'TITLE': 'Multi-Threaded Voronoi Demo',
  'GROUP': 'Demo'
}
