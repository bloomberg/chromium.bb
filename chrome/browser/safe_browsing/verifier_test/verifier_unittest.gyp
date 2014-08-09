{
  'targets': [
    {
      'target_name': 'verifier_test_dll_1',
      'type': 'loadable_module',
      'sources': [
        'verifier_test_dll.cc',
        'verifier_test_dll_1.def',
      ],
    },
    {
      'target_name': 'verifier_test_dll_2',
      'type': 'loadable_module',
      'sources': [
        'verifier_test_dll.cc',
        'verifier_test_dll_2.def',
      ],
    },
  ],
}