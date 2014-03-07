# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'include_tests%': 1,
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'cast_transport',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/cast/logging/logging.gyp:cast_common_logging',
        '<(DEPTH)/net/net.gyp:net',
        'utility/utility.gypi:transport_utility',
      ],
      'sources': [
        'cast_transport_config.cc',
        'cast_transport_config.h',
        'cast_transport_defines.h',
        'cast_transport_sender.h',
        'cast_transport_sender_impl.cc',
        'cast_transport_sender_impl.h',
        'pacing/paced_sender.cc',
        'pacing/paced_sender.h',
        'rtcp/rtcp_builder.cc',
        'rtcp/rtcp_builder.h',
        'rtp_sender/packet_storage/packet_storage.cc',
        'rtp_sender/packet_storage/packet_storage.h',
        'rtp_sender/rtp_packetizer/rtp_packetizer.cc',
        'rtp_sender/rtp_packetizer/rtp_packetizer.h',
        'rtp_sender/rtp_sender.cc',
        'rtp_sender/rtp_sender.h',
        'transport/udp_transport.cc',
        'transport/udp_transport.h',
        'transport_audio_sender.cc',
        'transport_audio_sender.h',
        'transport_video_sender.cc',
        'transport_video_sender.h',
      ], # source
    },
  ],  # targets,
}
