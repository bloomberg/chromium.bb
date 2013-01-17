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
    'Makefile',
    'example.js',
  ],
  'DEST': 'examples',
  'NAME': 'input_events',
  'TITLE': 'Input Events',
  'DESC': """The Input Events example shows how to handle input events in a
multi-threaded application.  The main thread converts input events to
non-pepper events and puts them on a queue. The worker thread pulls them off of
the queue, converts them to a string, and then uses CallOnMainThread so that
PostMessage can be send the result of the worker thread to the browser.""",
  'FOCUS': """Multi-threading, keyboard and mouse input, view change, and focus
events.""",
  'GROUP': 'API',
}

