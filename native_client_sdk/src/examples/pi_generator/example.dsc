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
  'DEST': 'examples',
  'NAME': 'pi_generator',
  'TITLE': 'Monte Carlo Estimate for Pi',
  'DESC': """
The Pi Generator example demonstrates creating a helper thread that
estimate pi using the Monte Carlo method while randomly putting 1,000,000,000
points inside a 2D square that shares two sides with a quarter circle.""",
  'FOCUS': 'Thread creation, 2D graphics, view change events.',
  'GROUP': 'Concepts'
}
