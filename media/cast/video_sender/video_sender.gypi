# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'codecs/vp8/vp8_encoder.gypi',
  ],
  'targets': [
    {
      'target_name': 'video_sender',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
         '<(DEPTH)/third_party/',
      ],
      'sources': [
        'video_encoder.h',
        'video_encoder.cc',
        'video_sender.h',
        'video_sender.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/media/cast/rtcp/rtcp.gyp:*',
        '<(DEPTH)/media/cast/net/rtp_sender/rtp_sender.gyp:*',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        'congestion_control',
        'cast_vp8_encoder',
      ],
    },
  ],
}
