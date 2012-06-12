{
  'TOOLS': ['newlib', 'glibc'],
  'TARGETS': [
    {
      'NAME' : 'pi_generator',
      'TYPE' : 'main',
      'SOURCES' : [
        'pi_generator.cc',
        'pi_generator.h',
        'pi_generator_module.cc'
      ]
    }
  ],
  'DEST': 'examples',
  'NAME': 'pi_generator',
}

