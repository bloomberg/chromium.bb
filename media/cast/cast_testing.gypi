# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //media/cast:test_support
      'target_name': 'cast_test_utility',
      'type': 'static_library',
      'include_dirs': [
         '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_net',
        'cast_receiver',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '<(DEPTH)/third_party/libyuv/libyuv.gyp:libyuv',
        '<(DEPTH)/third_party/mt19937ar/mt19937ar.gyp:mt19937ar',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        'test/fake_media_source.cc',
        'test/fake_media_source.h',
        'test/fake_single_thread_task_runner.cc',
        'test/fake_single_thread_task_runner.h',
        'test/skewed_single_thread_task_runner.cc',
        'test/skewed_single_thread_task_runner.h',
        'test/skewed_tick_clock.cc',
        'test/skewed_tick_clock.h',
	'test/loopback_transport.cc',
	'test/loopback_transport.h',
        'test/utility/audio_utility.cc',
        'test/utility/audio_utility.h',
        'test/utility/barcode.cc',
        'test/utility/barcode.h',
        'test/utility/default_config.cc',
        'test/utility/default_config.h',
        'test/utility/in_process_receiver.cc',
        'test/utility/in_process_receiver.h',
        'test/utility/input_builder.cc',
        'test/utility/input_builder.h',
        'test/utility/net_utility.cc',
        'test/utility/net_utility.h',
        'test/utility/standalone_cast_environment.cc',
        'test/utility/standalone_cast_environment.h',
        'test/utility/video_utility.cc',
        'test/utility/video_utility.h',
        'test/utility/udp_proxy.cc',
        'test/utility/udp_proxy.h',
      ], # source
    },
    {
      # GN version: //media/cast:cast_unittests
      'target_name': 'cast_unittests',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_net',
        'cast_receiver',
        'cast_sender',
        'cast_test_utility',
        # Not a true dependency. This is here to make sure the CQ can verify
        # the tools compile correctly.
        'cast_tools',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/testing/gmock.gyp:gmock',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        '<(DEPTH)/media/base/run_all_unittests.cc',
        'logging/encoding_event_subscriber_unittest.cc',
        'logging/serialize_deserialize_test.cc',
        'logging/logging_impl_unittest.cc',
        'logging/logging_raw_unittest.cc',
        'logging/receiver_time_offset_estimator_impl_unittest.cc',
        'logging/simple_event_subscriber_unittest.cc',
        'logging/stats_event_subscriber_unittest.cc',
        'net/cast_transport_sender_impl_unittest.cc',
        'net/pacing/mock_paced_packet_sender.cc',
        'net/pacing/mock_paced_packet_sender.h',
        'net/pacing/paced_sender_unittest.cc',
        'net/rtcp/mock_rtcp_receiver_feedback.cc',
        'net/rtcp/mock_rtcp_receiver_feedback.h',
        'net/rtcp/rtcp_receiver_unittest.cc',
        'net/rtcp/rtcp_sender_unittest.cc',
        'net/rtcp/rtcp_unittest.cc',
        'net/rtcp/receiver_rtcp_event_subscriber_unittest.cc',
# TODO(miu): The following two are test utility modules.  Rename/move the files.
        'net/rtcp/test_rtcp_packet_builder.cc',
        'net/rtcp/test_rtcp_packet_builder.h',
        'net/rtp/cast_message_builder_unittest.cc',
        'net/rtp/frame_buffer_unittest.cc',
        'net/rtp/framer_unittest.cc',
        'net/rtp/mock_rtp_payload_feedback.cc',
        'net/rtp/mock_rtp_payload_feedback.h',
        'net/rtp/packet_storage_unittest.cc',
        'net/rtp/receiver_stats_unittest.cc',
        'net/rtp/rtp_header_parser.cc',
        'net/rtp/rtp_header_parser.h',
        'net/rtp/rtp_packet_builder.cc',
        'net/rtp/rtp_parser_unittest.cc',
        'net/rtp/rtp_packetizer_unittest.cc',
        'net/rtp/rtp_receiver_defines.h',
        'net/udp_transport_unittest.cc',
        'receiver/audio_decoder_unittest.cc',
        'receiver/frame_receiver_unittest.cc',
        'receiver/video_decoder_unittest.cc',
        'sender/audio_encoder_unittest.cc',
        'sender/audio_sender_unittest.cc',
        'sender/congestion_control_unittest.cc',
        'sender/external_video_encoder_unittest.cc',
        'sender/video_encoder_impl_unittest.cc',
        'sender/video_sender_unittest.cc',
        'test/end2end_unittest.cc',
        'test/fake_receiver_time_offset_estimator.cc',
        'test/fake_receiver_time_offset_estimator.h',
        'test/fake_single_thread_task_runner.cc',
        'test/fake_single_thread_task_runner.h',
        'test/fake_video_encode_accelerator.cc',
        'test/fake_video_encode_accelerator.h',
        'test/utility/audio_utility_unittest.cc',
        'test/utility/barcode_unittest.cc',
      ], # source
    },
    {
      'target_name': 'cast_benchmarks',
      'type': '<(gtest_target_type)',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_net',
        'cast_receiver',
        'cast_sender',
        'cast_test_utility',
        '<(DEPTH)/base/base.gyp:test_support_base',
        '<(DEPTH)/net/net.gyp:net',
        '<(DEPTH)/testing/gtest.gyp:gtest',
      ],
      'sources': [
        'test/cast_benchmarks.cc',
        'test/fake_single_thread_task_runner.cc',
        'test/fake_single_thread_task_runner.h',
        'test/fake_video_encode_accelerator.cc',
        'test/fake_video_encode_accelerator.h',
        'test/utility/test_util.cc',
        'test/utility/test_util.h',
      ], # source
      'conditions': [
        ['os_posix==1 and OS!="mac" and OS!="ios" and use_allocator!="none"',
          {
            'dependencies': [
              '<(DEPTH)/base/allocator/allocator.gyp:allocator',
            ],
          }
        ],
      ],
    },
    {
      # This is a target for the collection of cast development tools.
      # They are built on bots but not shipped.
      'target_name': 'cast_tools',
      'type': 'none',
      'dependencies': [
        'cast_receiver_app',
        'cast_sender_app',
        'cast_simulator',
        'udp_proxy',
      ],
    },
    {
      'target_name': 'cast_receiver_app',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_net',
        'cast_receiver',
        'cast_test_utility',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/testing/gtest.gyp:gtest',
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
            '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
          ],
        }],
      ],
    },
    {
      'target_name': 'cast_sender_app',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_net',
        'cast_sender',
        'cast_test_utility',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '<(DEPTH)/third_party/opus/opus.gyp:opus',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        '<(DEPTH)/media/cast/test/sender.cc',
      ],
    },
    {
      'target_name': 'cast_simulator',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_net',
        'cast_network_model_proto',
        'cast_sender',
        'cast_test_utility',
        '<(DEPTH)/net/net.gyp:net_test_support',
        '<(DEPTH)/media/media.gyp:media',
        '<(DEPTH)/testing/gtest.gyp:gtest',
        '<(DEPTH)/third_party/ffmpeg/ffmpeg.gyp:ffmpeg',
        '<(DEPTH)/third_party/opus/opus.gyp:opus',
        '<(DEPTH)/ui/gfx/gfx.gyp:gfx_geometry',
      ],
      'sources': [
        '<(DEPTH)/media/cast/test/simulator.cc',
      ],
    },
    {
      # GN version: //media/cast/test/proto
      'target_name': 'cast_network_model_proto',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'test/proto/network_simulation_model.proto',
      ],
      'variables': {
        'proto_in_dir': 'test/proto',
        'proto_out_dir': 'media/cast/test/proto',
      },
      'includes': ['../../build/protoc.gypi'],
    },
    {
      # GN version: //media/cast:generate_barcode_video
      'target_name': 'generate_barcode_video',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_test_utility',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/media.gyp:media',
      ],
      'sources': [
        'test/utility/generate_barcode_video.cc',
      ],
    },
    {
      # GN version: //media/cast:generate_timecode_audio
      'target_name': 'generate_timecode_audio',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_base',
        'cast_net',
        'cast_test_utility',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/media.gyp:media',
      ],
      'sources': [
        'test/utility/generate_timecode_audio.cc',
      ],
    },
    {
      # GN version: //media/cast:udp_proxy
      'target_name': 'udp_proxy',
      'type': 'executable',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'dependencies': [
        'cast_test_utility',
        '<(DEPTH)/base/base.gyp:base',
        '<(DEPTH)/media/media.gyp:media',
      ],
      'sources': [
        'test/utility/udp_proxy_main.cc',
      ],
    }
  ], # targets
}
