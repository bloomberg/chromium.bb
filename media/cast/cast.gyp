# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'include_tests%': 1,
  },
  'targets': [
    {
      'target_name': 'cast_config',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'cast_config.h',
        'cast_config.cc',
      ], # source
    },
    {
      'target_name': 'cast_sender',
      'type': 'static_library',
      'dependencies': [
        'cast_config',
        'cast_sender.gyp:cast_sender_impl',
      ],
    },
    {
      'target_name': 'cast_receiver',
      'type': 'static_library',
      'dependencies': [
        'cast_config',
        'cast_receiver.gyp:cast_receiver_impl',
      ],
    },
  ],  # targets,
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'cast_unittest',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_sender',
            'cast_receiver',
            'rtcp/rtcp.gyp:cast_rtcp_test',
            '<(DEPTH)/base/base.gyp:run_all_unittests',
            '<(DEPTH)/net/net.gyp:net',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'sources': [
            'congestion_control/congestion_control_unittest.cc',
            'pacing/paced_sender_unittest.cc',
            'rtcp/rtcp_receiver_unittest.cc',
            'rtcp/rtcp_sender_unittest.cc',
            'rtcp/rtcp_unittest.cc',
          ], # source
        },
      ],  # targets
    }], # include_tests
  ],
}
