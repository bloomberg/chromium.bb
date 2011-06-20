# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'includes': [
    'app_base.gypi',
  ],
  'targets': [
    {
      'target_name': 'app_unittests',
      'type': 'executable',
      'dependencies': [
        'app_base',
        'app_resources',
        '../base/base.gyp:test_support_base',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../ui/ui.gyp:ui_base',
      ],
      'sources': [
        'run_all_unittests.cc',
        'sql/connection_unittest.cc',
        'sql/sqlite_features_unittest.cc',
        'sql/statement_unittest.cc',
        'sql/transaction_unittest.cc',
        'test_suite.cc',
        'test_suite.h',
      ],
      'include_dirs': [
        '..',
      ],
      'conditions': [
        ['os_posix==1 and OS!="mac"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'app_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/app/app_resources',
      },
      'actions': [
        {
          'action_name': 'app_resources',
          'variables': {
            'grit_grd_file': 'resources/app_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
