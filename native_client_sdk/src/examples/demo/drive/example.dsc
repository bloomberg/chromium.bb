{
  'TOOLS': ['newlib', 'glibc', 'pnacl'],
  'TARGETS': [
    {
      'NAME' : 'drive',
      'TYPE' : 'main',
      'SOURCES' : ['drive.cc'],
      'LIBS': ['jsoncpp', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'PRE': """
#
# We use the chrome.experimental.identity API, which requires the
# --enable-experimental-expension-apis flag.
#
CHROME_ARGS += --enable-experimental-extension-apis
""",
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/demo',
  'NAME': 'drive',
  'TITLE': 'Google Drive',
  'GROUP': 'Demo',
  'PERMISSIONS': [
    'experimental',
    'https://www.googleapis.com/*/drive/*',
    'https://*.googleusercontent.com/*'
  ]
}
