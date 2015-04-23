{
  'variables': {
    # Unconditionally disable incremental linking for these modules so that
    # their exports do not go through an ILT jmp stub.
    'incremental_chrome_dll': '0',  # 0 means no
    'msvs_debug_link_incremental': '1',  # 1 means /INCREMENTAL:NO
  },
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