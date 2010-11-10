# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    '../../../build/common.gypi',
    '../../common.gypi',
  ],
  'target_defaults': {
    'include_dirs': [
      '..',
    ],
  },
  'targets': [
    {
      'target_name': 'test_utils',
      'type': 'static_library',
      'sources': [
        'gflag_utils.cc',
        'gflag_utils.h',
        'instance_count_mixin.cc',
        'instance_count_mixin.h',
        'mock_com.h',
        'mock_win32.h',
        'mock_static.h',
        'mock_window_utils.h',
        'mshtml_mocks.h',
        'dispex_mocks.h',
        'nt_internals.cc',
        'nt_internals.h',
        'test_utils.cc',
        'test_utils.h',
      ],
      'dependencies': [
        'mshtml_mocks',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'all_dependent_settings': {
        'include_dirs': [
          '..'
        ],
      },
    },
    {
      'target_name': 'mshtml_mocks',
      'type': 'none',
      'sources': [
        'mshtml_mocks.h',
        'mshtml_mocks.py',
        'com_mock.py',
      ],
      'actions': [
        {
          'action_name': 'make_mshtml_mocks',
          'msvs_cygwin_shell': 0,
          'msvs_quote_cmd': 0,
          'inputs': [
            'mshtml_mocks.py',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/mshtml_mocks.gen',
          ],
          'action': [
            '<@(python)',
            'mshtml_mocks.py',
            '> "<(SHARED_INTERMEDIATE_DIR)/mshtml_mocks.gen"',
          ],
        },
      ],
      # All who use this need to be able to find the .gen file we generate.
      'all_dependent_settings': {
        'include_dirs': ['<(SHARED_INTERMEDIATE_DIR)'],
      },
    },
    {
      'target_name': 'test_utils_unittests',
      'type': 'executable',
      'sources': [
        'gflag_utils_unittest.cc',
        'instance_count_mixin_unittest.cc',
        'nt_internals_unittest.cc',
        'test_utils_unittest.cc',
        'test_utils_unittest_main.cc',
      ],
      'dependencies': [
        'test_utils',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'libraries': [
        'oleacc.lib',
        'shlwapi.lib',
      ],
    }
  ]
}
