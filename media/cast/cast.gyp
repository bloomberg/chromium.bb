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
      'target_name': 'cast_config',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        '<(DEPTH)/base/base.gyp:base',
      ],
      'sources': [
        'cast_config.cc',
        'cast_config.h',
        'cast_defines.h',
        'cast_environment.cc',
        'cast_environment.h',
        'logging/logging_defines.cc',
        'logging/logging_defines.h',
        'logging/logging_impl.cc',
        'logging/logging_impl.h',
        'logging/logging_raw.cc',
        'logging/logging_raw.h',
        'logging/logging_stats.cc',
        'logging/logging_stats.h',
        'logging/raw_event_subscriber.h',
        'logging/simple_event_subscriber.cc',
        'logging/simple_event_subscriber.h',
      ], # source
    },
    {
      'target_name': 'cast_logging_proto_lib',
      'type': 'static_library',
      'sources': [
        'logging/proto/proto_utils.cc',
        'logging/proto/raw_events.proto',
      ],
      'variables': {
        'proto_in_dir': 'logging/proto',
        'proto_out_dir': 'media/cast/logging/proto',
      },
      'includes': ['../../build/protoc.gypi'],
    },
    {
      'target_name': 'sender_logging',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_config',
        'cast_logging_proto_lib',
        '<(DEPTH)/base/base.gyp:base',
      ],
      'export_dependent_settings': [
        'cast_logging_proto_lib',
      ],
      'sources': [
        'logging/encoding_event_subscriber.cc',
        'logging/encoding_event_subscriber.h',
        'logging/log_serializer.cc',
        'logging/log_serializer.h',
      ], # source
    },
  ],  # targets,
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'cast_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_config',
            'cast_logging_proto_lib',
            'cast_receiver.gyp:cast_receiver',
            'cast_sender.gyp:cast_sender',
            'sender_logging',
            'test/utility/utility.gyp:cast_test_utility',
            'transport/cast_transport.gyp:cast_transport',
            '<(DEPTH)/base/base.gyp:run_all_unittests',
            '<(DEPTH)/base/base.gyp:test_support_base',
            '<(DEPTH)/net/net.gyp:net',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '<(DEPTH)/',
            '<(DEPTH)/third_party/',
            '<(DEPTH)/third_party/webrtc/',
          ],
          'sources': [
            'audio_receiver/audio_decoder_unittest.cc',
            'audio_receiver/audio_receiver_unittest.cc',
            'audio_sender/audio_encoder_unittest.cc',
            'audio_sender/audio_sender_unittest.cc',
            'congestion_control/congestion_control_unittest.cc',
            'framer/cast_message_builder_unittest.cc',
            'framer/frame_buffer_unittest.cc',
            'framer/framer_unittest.cc',
            'logging/encoding_event_subscriber_unittest.cc',
            'logging/logging_impl_unittest.cc',
            'logging/logging_raw_unittest.cc',
            'logging/simple_event_subscriber_unittest.cc',
            'rtcp/mock_rtcp_receiver_feedback.cc',
            'rtcp/mock_rtcp_receiver_feedback.h',
            'rtcp/mock_rtcp_sender_feedback.cc',
            'rtcp/mock_rtcp_sender_feedback.h',
            'rtcp/rtcp_receiver_unittest.cc',
            'rtcp/rtcp_sender_unittest.cc',
            'rtcp/rtcp_unittest.cc',
            'rtcp/receiver_rtcp_event_subscriber_unittest.cc',
            'rtcp/sender_rtcp_event_subscriber_unittest.cc',
            'rtp_receiver/rtp_receiver_defines.h',
            'rtp_receiver/mock_rtp_payload_feedback.cc',
            'rtp_receiver/mock_rtp_payload_feedback.h',
            'rtp_receiver/receiver_stats_unittest.cc',
            'rtp_receiver/rtp_parser/test/rtp_packet_builder.cc',
            'rtp_receiver/rtp_parser/rtp_parser_unittest.cc',
            'test/encode_decode_test.cc',
            'test/end2end_unittest.cc',
            'test/fake_gpu_video_accelerator_factories.cc',
            'test/fake_gpu_video_accelerator_factories.h',
            'test/fake_single_thread_task_runner.cc',
            'test/fake_single_thread_task_runner.h',
            'test/fake_video_encode_accelerator.cc',
            'test/fake_video_encode_accelerator.h',
            'transport/pacing/mock_paced_packet_sender.cc',
            'transport/pacing/mock_paced_packet_sender.h',
            'transport/pacing/paced_sender_unittest.cc',
            'transport/rtp_sender/packet_storage/packet_storage_unittest.cc',
            'transport/rtp_sender/rtp_packetizer/rtp_packetizer_unittest.cc',
            'transport/rtp_sender/rtp_packetizer/test/rtp_header_parser.cc',
            'transport/rtp_sender/rtp_packetizer/test/rtp_header_parser.h',
            'transport/transport/udp_transport_unittest.cc',
            'video_receiver/video_decoder_unittest.cc',
            'video_receiver/video_receiver_unittest.cc',
            'video_sender/external_video_encoder_unittest.cc',
            'video_sender/video_encoder_impl_unittest.cc',
            'video_sender/video_sender_unittest.cc',
          ], # source
        },
        {
          'target_name': 'cast_sender_app',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_config',
            'sender_logging',
            '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
            '<(DEPTH)/net/net.gyp:net_test_support',
            '<(DEPTH)/media/cast/cast_sender.gyp:*',
            '<(DEPTH)/media/media.gyp:media',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/opus/opus.gyp:opus',
            '<(DEPTH)/media/cast/transport/cast_transport.gyp:cast_transport',
            '<(DEPTH)/media/cast/test/utility/utility.gyp:cast_test_utility',
          ],
          'sources': [
            '<(DEPTH)/media/cast/test/sender.cc',
          ],
        },
        {
          'target_name': 'cast_receiver_app',
          'type': 'executable',
          'include_dirs': [
            '<(DEPTH)/',
          ],
          'dependencies': [
            'cast_config',
            '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
            '<(DEPTH)/net/net.gyp:net_test_support',
            '<(DEPTH)/media/cast/cast_receiver.gyp:*',
            '<(DEPTH)/media/media.gyp:media',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/media/cast/transport/cast_transport.gyp:cast_transport',
            '<(DEPTH)/media/cast/test/utility/utility.gyp:cast_test_utility',
            '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
          ],
          'sources': [
            '<(DEPTH)/media/cast/test/receiver.cc',
          ],
          'conditions': [
            ['OS == "linux" and use_x11==1', {
              'dependencies': [
                '<(DEPTH)/build/linux/system.gyp:x11',
                '<(DEPTH)/build/linux/system.gyp:xext',
              ],
              'sources': [
                '<(DEPTH)/media/cast/test/linux_output_window.cc',
                '<(DEPTH)/media/cast/test/linux_output_window.h',
              ],
          }],
          ],
        },
      ],  # targets
    }], # include_tests
  ],
}
