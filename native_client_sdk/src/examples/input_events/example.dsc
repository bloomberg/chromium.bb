{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'linux', 'win'],
  'TARGETS': [
    {
      'NAME' : 'input_events',
      'TYPE' : 'main',
      'SOURCES' : [
        'custom_events.cc',
        'custom_events.h',
        'input_events.cc',
        'shared_queue.h',
      ],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples',
  'NAME': 'input_events',
  'TITLE': 'Input Events',
  'GROUP': 'API',
}

