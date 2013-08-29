# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_rtcp',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'rtcp_defines.h',
        'rtcp.h',
        'rtcp.cc',
        'rtcp_receiver.cc',
        'rtcp_receiver.h',
        'rtcp_sender.cc',
        'rtcp_sender.h',
        'rtcp_utility.cc',
        'rtcp_utility.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
    },
    {
      'target_name': 'cast_rtcp_test',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'test_rtcp_packet_builder.cc',
        'test_rtcp_packet_builder.h',
      ], # source
      'dependencies': [
        'cast_rtcp',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
    },
  ],
}

