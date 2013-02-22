{
  'TOOLS': ['newlib'],
  'SEARCH': [
    '.',
    '..',
    '../../tools',
  ],
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
      'CCFLAGS': ['-fno-omit-frame-pointer'],
      'LIBS' : ['ppapi', 'pthread']
    }
  ],

  # The debugging example needs to use a different HTTP server to handle POST
  # messages from the NaCl module.
  'PRE': """
CHROME_ARGS+=--no-sandbox
CHROME_ENV:=NACL_DANGEROUS_ENABLE_FILE_ACCESS=1
CHROME_ENV+=NACL_SECURITY_DISABLE=1
CHROME_ENV+=NACL_UNTRUSTED_EXCEPTION_HANDLING=1
""",

  'DATA': [
    'example.js',
    'handler.py'
  ],
  'DEST': 'examples',
  'NAME': 'debugging',
  'TITLE': 'Debugging',
  'DESC': """
Debugging example shows how to use developer only features to enable
catching an exception, and then using that to create a stacktrace.""",
  'FOCUS': 'Debugging, Stacktraces.',
  'GROUP': 'Concepts'
}

