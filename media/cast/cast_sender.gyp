# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'audio_sender/audio_sender.gypi',
    'congestion_control/congestion_control.gypi',
    'video_sender/video_sender.gypi',
  ],
  'targets': [
    {
      'target_name': 'cast_sender',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
        '<(DEPTH)/third_party/webrtc/',
      ],
      'sources': [
        'cast_sender.h',
        'cast_sender_impl.cc',
        'cast_sender_impl.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/crypto/crypto.gyp:crypto',
        'audio_sender',
        'congestion_control',
        'net/pacing/paced_sender.gyp:cast_paced_sender',
        'net/rtp_sender/rtp_sender.gyp:cast_rtp_sender',
        'rtcp/rtcp.gyp:cast_rtcp',
        'video_sender',
      ], # dependencies
    },
  ],
}
