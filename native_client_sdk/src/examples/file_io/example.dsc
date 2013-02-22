{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'file_io',
      'TYPE' : 'main',
      'SOURCES' : ['file_io.cc'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples',
  'NAME': 'file_io',
  'TITLE': 'File I/O',
  'DESC': """
The File IO example demonstrates saving, loading, and deleting files
from the persistent file store.""",
  'FOCUS': 'File input and output.',
  'GROUP': 'API'
}

