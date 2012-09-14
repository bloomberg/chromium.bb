{
  'TOOLS': ['newlib'],
  'TARGETS': [
    {
      'NAME' : 'debugging',
      'TYPE' : 'main',
      'SOURCES' : [
        'hello_world.c', 
        'string_stream.c', 
        'string_stream.h',
        'untrusted_crash_dump.c',
        'untrusted_crash_dump.h'
      ],
      'CCFLAGS': ['$(NACL_CCFLAGS)', '-fno-omit-frame-pointer'],
      'LIBS' : ['ppapi', 'pthread']
    }
  ],
  'POST': 'include Makefile.inc\n',
  'DATA': ['Makefile.inc', 'example.js'],
  'DEST': 'examples',
  'NAME': 'debugging',
  'TITLE': 'Debugging',
  'DESC': """
Debugging example shows how to use developer only features to enable
catching an exception, and then using that to create a stacktrace.""",
  'INFO': 'Debugging, Stacktraces.'

}

