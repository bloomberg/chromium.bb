{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win'],
  'TARGETS': [
    {
      'NAME' : 'pi_generator',
      'TYPE' : 'main',
      'SOURCES' : ['pi_generator.cc'],
      'LIBS': ['ppapi_simple', 'nacl_io', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
  ],
  'DEST': 'examples/demo',
  'NAME': 'pi_generator',
  'TITLE': 'Monte Carlo Estimate for Pi',
  'GROUP': 'Demo'
}
