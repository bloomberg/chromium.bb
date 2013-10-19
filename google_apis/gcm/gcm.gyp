# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },

  'targets': [
    # The public GCM shared library target.
    {
      'target_name': 'gcm',
      'type': 'shared_library',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '../..',
      ],
      'defines': [
        'GCM_IMPLEMENTATION',
      ],
      'export_dependent_settings': [
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../net/net.gyp:net',
        '../../third_party/protobuf/protobuf.gyp:protobuf_lite'
      ],
      'sources': [
        'base/socket_stream.h',
        'base/socket_stream.cc',
      ],
    },

    # The main GCM unit tests.
    {
      'target_name': 'gcm_unit_tests',
      'type': '<(gtest_target_type)',
      'variables': { 'enable_wexit_time_destructors': 1, },
      'include_dirs': [
        '../..',
      ],
      'dependencies': [
        '../../base/base.gyp:run_all_unittests',
        '../../base/base.gyp:base',
        '../../net/net.gyp:net_test_support',
        '../../testing/gtest.gyp:gtest',
        'gcm'
      ],
      'sources': [
        'base/socket_stream_unittest.cc',
      ]
    },
  ],
}
