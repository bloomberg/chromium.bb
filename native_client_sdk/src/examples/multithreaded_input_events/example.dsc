{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'mt_input_events',
      'TYPE' : 'main',
      'SOURCES' : [
        'custom_events.cc',
        'custom_events.h',
        'mt_input_events.cc',
        'shared_queue.h',
        'thread_safe_ref_count.h'
      ]
    }
  ],
  'DEST': 'examples',
  'NAME': 'mt_input_events',
}

