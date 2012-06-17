{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'file_histogram',
      'TYPE' : 'main',
      'SOURCES' : ['file_histogram.cc'],
    }
  ],
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

