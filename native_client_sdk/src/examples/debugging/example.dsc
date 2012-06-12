{
  'TOOLS': ['newlib', 'glibc'],
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
      'CCFLAGS': ['$(NACL_CCFLAGS)', '-fno-omit-frame-pointer']
    }
  ],
  'POST': 'include Makefile.inc\n',
  'DATA': ['Makefile.inc'],
  'DEST': 'examples',
  'NAME': 'debugging'
}

