# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'include_tests%': 1,
    'chromium_code': 1,
  },
  'targets': [
  ],  # targets,
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'cast_unittests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_config.gyp:cast_config',
            'cast_receiver.gyp:cast_receiver',
            'cast_sender.gyp:cast_sender',
            'logging/logging.gyp:cast_log_analysis',
            'logging/logging.gyp:cast_logging_proto_lib',
            'logging/logging.gyp:sender_logging',
            'test/utility/utility.gyp:cast_test_utility',
            'transport/cast_transport.gyp:cast_transport',
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
            '<(DEPTH)/media/base/run_all_unittests.cc',
            'audio_receiver/audio_decoder_unittest.cc',
            'audio_receiver/audio_receiver_unittest.cc',
            'audio_sender/audio_encoder_unittest.cc',
            'audio_sender/audio_sender_unittest.cc',
            'congestion_control/congestion_control_unittest.cc',
            'framer/cast_message_builder_unittest.cc',
            'framer/frame_buffer_unittest.cc',
            'framer/framer_unittest.cc',
            'logging/encoding_event_subscriber_unittest.cc',
            'logging/serialize_deserialize_test.cc',
            'logging/logging_impl_unittest.cc',
            'logging/logging_raw_unittest.cc',
            'logging/simple_event_subscriber_unittest.cc',
            'logging/stats_event_subscriber_unittest.cc',
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
            'test/end2end_unittest.cc',
            'test/fake_single_thread_task_runner.cc',
            'test/fake_single_thread_task_runner.h',
            'test/fake_video_encode_accelerator.cc',
            'test/fake_video_encode_accelerator.h',
            'test/utility/audio_utility_unittest.cc',
            'test/utility/barcode_unittest.cc',
            'transport/cast_transport_sender_impl_unittest.cc',
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
            'cast_config.gyp:cast_config',
            'logging/logging.gyp:sender_logging',
            '<(DEPTH)/ui/gfx/gfx.gyp:gfx',
            '<(DEPTH)/net/net.gyp:net_test_support',
            '<(DEPTH)/media/cast/cast_sender.gyp:*',
            '<(DEPTH)/media/media.gyp:media',
            '<(DEPTH)/testing/gtest.gyp:gtest',
            '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
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
            'cast_config.gyp:cast_config',
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
