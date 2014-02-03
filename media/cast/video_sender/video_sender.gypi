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
      ],
      'sources': [
        'external_video_encoder.h',
        'external_video_encoder.cc',
        'video_encoder.h',
        'video_encoder_impl.h',
        'video_encoder_impl.cc',
        'video_sender.h',
        'video_sender.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/media/cast/rtcp/rtcp.gyp:*',
        '<(DEPTH)/media/cast/transport/cast_transport.gyp:cast_transport',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/media/media.gyp:shared_memory_support',
        'congestion_control',
        'cast_vp8_encoder',
      ],
    },
  ],
}
