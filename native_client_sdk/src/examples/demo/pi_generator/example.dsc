{
  'TOOLS': ['newlib', 'glibc', 'pnacl', 'win', 'linux'],
  'TARGETS': [
    {
      'NAME' : 'pi_generator',
      'TYPE' : 'main',
      'SOURCES' : [
        'pi_generator.cc',
        'pi_generator.h',
        'pi_generator_module.cc'
      ],
      'LIBS': ['ppapi_cpp', 'ppapi', 'pthread']
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
