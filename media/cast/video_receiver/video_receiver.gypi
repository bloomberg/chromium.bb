# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of the source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_video_receiver',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
         '<(DEPTH)/third_party/',
         '<(DEPTH)/third_party/webrtc',
      ],
      'sources': [
        'video_decoder.h',
        'video_decoder.cc',
        'video_receiver.h',
        'video_receiver.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/cast/framer/framer.gyp:cast_framer',
        '<(DEPTH)/media/cast/rtp_receiver/rtp_receiver.gyp:cast_rtp_receiver',
        '<(DEPTH)/media/cast/transport/utility/utility.gyp:transport_utility',
        '<(DEPTH)/third_party/libvpx/libvpx.gyp:libvpx',
      ],
    },
  ],
}


