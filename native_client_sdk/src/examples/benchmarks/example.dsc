{
  'TOOLS': ['pnacl'],
  'SEARCH': [
    '.',
    '../../resources',
    '../../../../native_client/tests/benchmark',
    '../../../../native_client/src',
  ],
  'TARGETS': [
    {
      'NAME' : 'benchmarks',
      'TYPE' : 'main',
      'SOURCES' : [
        'benchmark_binarytrees_c.c',
        'benchmark_binarytrees.cc',
        'benchmark_chameneos_c.c',
        'benchmark_chameneos.cc',
        'benchmark_life.cc',
        'benchmark_nbody_c.c',
        'benchmark_nbody.cc',
        'framework.cc',
        'framework.h',
        'framework_ppapi.cc',
        'thread_pool.h',
        'thread_pool.cc'
      ],
      'DEPS': ['ppapi_simple', 'nacl_io'],
      'LIBS': ['ppapi_simple', 'nacl_io', 'sdk_util', 'ppapi_cpp', 'ppapi', 'pthread']
    }
  ],
  'DATA': [
    'example.js',
    'third_party/computer_language_benchmarks_game/binarytrees.c',
    'third_party/computer_language_benchmarks_game/chameneos.c',
    'third_party/computer_language_benchmarks_game/nbody.c',
  ],
  'DEST': 'examples/benchmarks',
  'NAME': 'benchmarks',
  'TITLE': "Benchmark Suite",
  'GROUP': 'Benchmarks'
}
