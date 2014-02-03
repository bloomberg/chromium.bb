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
      ],
      'sources': [
        'cast_sender.h',
        'cast_sender_impl.cc',
        'cast_sender_impl.h',
      ], # source
      'dependencies': [
        '<(DEPTH)/media/cast/transport/cast_transport.gyp:cast_transport',
        'audio_sender',
        'congestion_control',
        'rtcp/rtcp.gyp:cast_rtcp',
        'video_sender',
      ], # dependencies
    },
  ],
}
