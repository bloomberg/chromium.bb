# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromecast_branding%': 'Chromium',
  },
  'targets': [
    {
      'target_name': 'media_base',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
      ],
      'sources': [
        'base/key_systems_common.cc',
        'base/key_systems_common.h',
      ],
      'conditions': [
        ['chromecast_branding=="Chrome"', {
          'dependencies': [
            'internal/chromecast_internal.gyp:media_base_internal',
          ],
        }, {
          'sources': [
            'base/key_systems_common_simple.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'cma_base',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cma/base/balanced_media_task_runner_factory.cc',
        'cma/base/balanced_media_task_runner_factory.h',
        'cma/base/buffering_controller.cc',
        'cma/base/buffering_controller.h',
        'cma/base/buffering_frame_provider.cc',
        'cma/base/buffering_frame_provider.h',
        'cma/base/buffering_state.cc',
        'cma/base/buffering_state.h',
        'cma/base/cma_logging.h',
        'cma/base/coded_frame_provider.cc',
        'cma/base/coded_frame_provider.h',
        'cma/base/decoder_buffer_adapter.cc',
        'cma/base/decoder_buffer_adapter.h',
        'cma/base/decoder_buffer_base.cc',
        'cma/base/decoder_buffer_base.h',
        'cma/base/media_task_runner.cc',
        'cma/base/media_task_runner.h',
      ],
    },
    {
      'target_name': 'cma_ipc',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'cma/ipc/media_memory_chunk.cc',
        'cma/ipc/media_memory_chunk.h',
        'cma/ipc/media_message.cc',
        'cma/ipc/media_message.h',
        'cma/ipc/media_message_fifo.cc',
        'cma/ipc/media_message_fifo.h',
      ],
    },
    {
      'target_name': 'cma_ipc_streamer',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
        'cma_base',
      ],
      'sources': [
        'cma/ipc_streamer/audio_decoder_config_marshaller.cc',
        'cma/ipc_streamer/audio_decoder_config_marshaller.h',
        'cma/ipc_streamer/av_streamer_proxy.cc',
        'cma/ipc_streamer/av_streamer_proxy.h',
        'cma/ipc_streamer/coded_frame_provider_host.cc',
        'cma/ipc_streamer/coded_frame_provider_host.h',
        'cma/ipc_streamer/decoder_buffer_base_marshaller.cc',
        'cma/ipc_streamer/decoder_buffer_base_marshaller.h',
        'cma/ipc_streamer/decrypt_config_marshaller.cc',
        'cma/ipc_streamer/decrypt_config_marshaller.h',
        'cma/ipc_streamer/video_decoder_config_marshaller.cc',
        'cma/ipc_streamer/video_decoder_config_marshaller.h',
      ],
    },
    {
      'target_name': 'cma_filters',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
        'cma_base',
      ],
      'sources': [
        'cma/filters/demuxer_stream_adapter.cc',
        'cma/filters/demuxer_stream_adapter.h',
      ],
    },
    {
      'target_name': 'cast_media',
      'type': 'none',
      'dependencies': [
        'cma_base',
        'cma_filters',
        'cma_ipc',
        'cma_ipc_streamer',
      ],
    },
    {
      'target_name': 'cast_media_unittests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        'cast_media',
        '../../base/base.gyp:base',
        '../../base/base.gyp:base_i18n',
        '../../base/base.gyp:test_support_base',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
      ],
      'sources': [
        'cma/base/balanced_media_task_runner_unittest.cc',
        'cma/base/buffering_controller_unittest.cc',
        'cma/base/buffering_frame_provider_unittest.cc',
        'cma/base/frame_generator_for_test.cc',
        'cma/base/frame_generator_for_test.h',
        'cma/base/mock_frame_consumer.cc',
        'cma/base/mock_frame_consumer.h',
        'cma/base/mock_frame_provider.cc',
        'cma/base/mock_frame_provider.h',
        'cma/base/run_all_unittests.cc',
        'cma/filters/demuxer_stream_adapter_unittest.cc',
        'cma/ipc/media_message_fifo_unittest.cc',
        'cma/ipc/media_message_unittest.cc',
        'cma/ipc_streamer/av_streamer_unittest.cc',
      ],
    },
  ],
}
