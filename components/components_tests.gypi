# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'components_unittests',
      'type': '<(gtest_target_type)',
      'sources': [
        'test/run_all_unittests.cc',
      ],
      'dependencies': [
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
      ],
    }
  ]
}
