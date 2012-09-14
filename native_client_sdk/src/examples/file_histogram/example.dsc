{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'file_histogram',
      'TYPE' : 'main',
      'SOURCES' : ['file_histogram.cc'],
      'LIBS' : ['ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': ['example.js'],
  'DEST': 'examples',
  'NAME': 'file_histogram',
  'TITLE': 'File Histogram.',
  'DESC': """
The File Histogram example demonstrates prompting the user for a file,
passing the file contents to NativeClient as a VarArrayBuffer, then drawing a
histogram representing the contents of the file to a 2D square.
""",
  'INFO': 'Teaching focus: VarArrayBuffer, 2D, File input.'
}

