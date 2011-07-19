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
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        '../sql/run_all_unittests.cc',
        '../sql/connection_unittest.cc',
        '../sql/sqlite_features_unittest.cc',
        '../sql/statement_unittest.cc',
        '../sql/transaction_unittest.cc',
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
  ],
}
