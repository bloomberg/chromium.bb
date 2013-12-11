# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'audio_sender',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
      ],
      'sources': [
        'audio_encoder.h',
        'audio_encoder.cc',
        'audio_sender.h',
        'audio_sender.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        '<(DEPTH)/media/cast/rtcp/rtcp.gyp:cast_rtcp',
        '<(DEPTH)/media/cast/net/rtp_sender/rtp_sender.gyp:*',
        '<(DEPTH)/third_party/opus/opus.gyp:opus',
      ],
    },
  ],
}


