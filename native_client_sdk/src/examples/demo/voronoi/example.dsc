{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'voronoi',
      'TYPE' : 'main',
      'SOURCES' : [
        'voronoi.cc',
        'threadpool.cc',
        'threadpool.h'
      ],

      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
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
