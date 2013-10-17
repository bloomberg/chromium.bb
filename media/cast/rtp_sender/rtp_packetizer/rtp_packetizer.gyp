# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_rtp_packetizer',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
        '<(DEPTH)/third_party/webrtc/',
      ],
      'sources': [
        'rtp_packetizer.cc',
        'rtp_packetizer.h',
        'rtp_packetizer_config.cc',
        'rtp_packetizer_config.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/net/net.gyp:net',
      ],
    },
  ],
}
