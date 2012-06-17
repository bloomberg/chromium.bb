{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'load_progress',
      'TYPE' : 'main',
      'SOURCES' : ['load_progress.cc'],
    }
  ],
  'DATA': ['check_browser.js'],
  'DEST': 'examples',
  'NAME': 'load_progress',
  'TITLE': 'Load Progress',
  'DESC': """
The Load Progress example demonstrates how to listen for and handle 
events that occur while a NaCl module loads.  This example listens for
different load event types and dispatches different events to their
respective handler. This example also checks for valid browser version and
shows how to calculate and display loading progress.""",
  'INFO': 'Progress event handling.'
}

