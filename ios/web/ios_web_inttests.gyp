# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ios_web_inttests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        '../../ui/base/ui_base.gyp:ui_base_test_support',
        'ios_web.gyp:ios_web',
        'ios_web.gyp:ios_web_test_support',
      ],
      'sources': [
        'browser_state_web_view_partition_inttest.mm',
        'public/test/http_server_inttest.mm',
        'test/run_all_unittests.cc',
      ],
    },
  ],
}
