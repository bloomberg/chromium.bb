# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../build/common.gypi',
    '../../ceee/common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '../..',
    ],
  },
  'targets': [
    {
      'target_name': 'initializing_coclass',
      'type': 'none',
      'sources': [
        'initializing_coclass.h',
        'initializing_coclass.py',
      ],
      'actions': [
        {
          'action_name': 'make_initializing_coclass',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'inputs': [
            'initializing_coclass.py',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/initializing_coclass_gen.inl',
          ],
          'action': [
            '<@(python)',
            'initializing_coclass.py',
            '"<(SHARED_INTERMEDIATE_DIR)/initializing_coclass_gen.inl"',
          ],
        },
      ],
      # All who use this need to be able to find the .inl file we generate.
      'all_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
    {
      'target_name': 'ceee_common',
      'type': 'static_library',
      'dependencies': [
        'initializing_coclass',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
      ],
      'msvs_guid': '4BAB6049-29A7-47C2-A754-14CA3FBF5BD9',
      'sources': [
        'com_utils.cc',
        'com_utils.h',
        'initializing_coclass.h',
        'install_utils.cc',
        'install_utils.h',
        'npobject_impl.cc',
        'npobject_impl.h',
        'npplugin_impl.cc',
        'npplugin_impl.h',
        'process_utils_win.cc',
        'process_utils_win.h',
        'sidestep_integration.cc',
        'windows_constants.cc',
        'windows_constants.h',
        'window_utils.cc',
        'window_utils.h',
      ],
    },
    {
      'target_name': 'ceee_common_unittests',
      'type': 'executable',
      'msvs_guid': 'A6DF7E24-63DF-47E6-BA72-423FCA8AC36D',
      'sources': [
        'com_utils_unittest.cc',
        'common_unittest_main.cc',
        'initializing_coclass_unittest.cc',
        'install_utils_unittest.cc',
        'npobject_impl_unittest.cc',
        'npplugin_impl_unittest.cc',
        'process_utils_win_unittest.cc',
        'sidestep_unittest.cc',
        'window_utils_unittest.cc',
      ],
      'dependencies': [
        'ceee_common',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/ceee/testing/sidestep/sidestep.gyp:sidestep',
        '<(DEPTH)/ceee/testing/utils/test_utils.gyp:test_utils',
        '<(DEPTH)/chrome/chrome.gyp:installer_util',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/npapi/npapi.gyp:npapi',
      ],
    }
  ]
}
