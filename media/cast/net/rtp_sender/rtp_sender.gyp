# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_rtp_sender',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
        '<(DEPTH)/third_party/webrtc/',
      ],
      'sources': [
        'rtp_sender.cc',
        'rtp_sender.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        'packet_storage/packet_storage.gyp:*',
        'rtp_packetizer/rtp_packetizer.gyp:*',
      ],
    },
  ],
}
