{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'mt_input_events',
      'TYPE' : 'main',
      'SOURCES' : [
        'custom_events.cc',
        'custom_events.h',
        'mt_input_events.cc',
        'shared_queue.h',
      ],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': ['example.js'],
  'DEST': 'examples',
  'NAME': 'mt_input_events',
  'TITLE': 'Multi-threaded Input Events',
  'DESC': """
The Multithreaded Input Events example combines HTML, Javascript,
and C++ (the C++ is compiled to create a .nexe file).
The C++ shows how to handle input events in a multi-threaded application.
The main thread converts input events to non-pepper events and puts them on
a queue. The worker thread pulls them off of the queue, converts them to a
string, and then uses CallOnMainThread so that PostMessage can be send the
result of the worker thread to the browser.""",
  'INFO': 'Multithreaded event handling.'
}

