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
        '<(DEPTH)/third_party/webrtc',
      ],
      'sources': [
        'audio_encoder.h',
        'audio_encoder.cc',
        'audio_sender.h',
        'audio_sender.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/media/cast/rtcp/rtcp.gyp:cast_rtcp',
        '<(DEPTH)/media/cast/rtp_sender/rtp_sender.gyp:*',
        '<(DEPTH)/third_party/webrtc/webrtc.gyp:webrtc',
      ],
    },
  ],
}


