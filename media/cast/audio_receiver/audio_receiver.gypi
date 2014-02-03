# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'cast_audio_receiver',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
        '<(DEPTH)/third_party/',
        '<(DEPTH)/third_party/webrtc/',
      ],
      'sources': [
        'audio_decoder.h',
        'audio_decoder.cc',
        'audio_receiver.h',
        'audio_receiver.cc',
      ], # source
      'dependencies': [
        '<(DEPTH)/media/cast/transport/utility/utility.gypi:transport_utility',
        '<(DEPTH)/media/cast/rtcp/rtcp.gyp:cast_rtcp',
        '<(DEPTH)/media/cast/rtp_receiver/rtp_receiver.gyp:cast_rtp_receiver',
        '<(DEPTH)/third_party/webrtc/webrtc.gyp:webrtc',
      ],
    },
  ],
}
