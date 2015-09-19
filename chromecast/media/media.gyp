# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'chromecast_branding%': 'public',
    'libcast_media_gyp%': '',
    'use_default_libcast_media%': 1,
  },
  'target_defaults': {
    'include_dirs': [
      '../public/', # Public APIs
    ],
  },
  'targets': [
    # TODO(slan): delete this target once Chromecast M44/earlier is obsolete.
    # See: b/21639416
    {
      'target_name': 'libffmpegsumo',
      'type': 'loadable_module',
      'sources': ['empty.cc'],
    },
    {
      'target_name': 'media_base',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../crypto/crypto.gyp:crypto',
        '../../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
        '<(libcast_media_gyp):libcast_media_1.0',
      ],
      'sources': [
        'base/decrypt_context_impl.cc',
        'base/decrypt_context_impl.h',
        'base/decrypt_context_impl_clearkey.cc',
        'base/decrypt_context_impl_clearkey.h',
        'base/key_systems_common.cc',
        'base/key_systems_common.h',
        'base/media_caps.cc',
        'base/media_caps.h',
        'base/media_codec_support.cc',
        'base/media_codec_support.h',
        'base/media_message_loop.cc',
        'base/media_message_loop.h',
        'base/switching_media_renderer.cc',
        'base/switching_media_renderer.h',
      ],
      'conditions': [
        ['chromecast_branding!="public"', {
          'dependencies': [
            '../internal/chromecast_internal.gyp:media_base_internal',
          ],
        }, {
          'sources': [
            'base/key_systems_common_simple.cc',
          ],
        }],
      ],
    },
    {
      'target_name': 'media_cdm',
      'type': '<(component)',
      'dependencies': [
        'media_base',
        '../../base/base.gyp:base',
        '../../media/media.gyp:media',
      ],
      'sources': [
        'cdm/browser_cdm_cast.cc',
        'cdm/browser_cdm_cast.h',
        'cdm/chromecast_init_data.cc',
        'cdm/chromecast_init_data.h',
      ],
      'conditions': [
        ['use_playready==1', {
          'sources': [
            'cdm/playready_drm_delegate_android.cc',
            'cdm/playready_drm_delegate_android.h',
          ],
        }],
      ],
    },
    {
      'target_name': 'cma_base',
      'type': '<(component)',
      'dependencies': [
        '../chromecast.gyp:cast_base',
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
        'cma/base/buffering_defs.cc',
        'cma/base/buffering_defs.h',
        'cma/base/buffering_frame_provider.cc',
        'cma/base/buffering_frame_provider.h',
        'cma/base/buffering_state.cc',
        'cma/base/buffering_state.h',
        'cma/base/cast_decoder_buffer_impl.cc',
        'cma/base/cast_decoder_buffer_impl.h',
        'cma/base/cast_decrypt_config_impl.cc',
        'cma/base/cast_decrypt_config_impl.h',
        'cma/base/cma_logging.h',
        'cma/base/coded_frame_provider.cc',
        'cma/base/coded_frame_provider.h',
        'cma/base/decoder_buffer_adapter.cc',
        'cma/base/decoder_buffer_adapter.h',
        'cma/base/decoder_config_adapter.cc',
        'cma/base/decoder_config_adapter.h',
        'cma/base/media_task_runner.cc',
        'cma/base/media_task_runner.h',
        'cma/base/simple_media_task_runner.cc',
        'cma/base/simple_media_task_runner.h',         
      ],
    },
    {
      'target_name': 'default_cma_backend',
      'type': '<(component)',
      'dependencies': [
        '../chromecast.gyp:cast_base',
        '../../base/base.gyp:base',
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'cma/backend/audio_pipeline_device_default.cc',
        'cma/backend/audio_pipeline_device_default.h',
        'cma/backend/media_clock_device_default.cc',
        'cma/backend/media_clock_device_default.h',
        'cma/backend/media_component_device_default.cc',
        'cma/backend/media_component_device_default.h',
        'cma/backend/media_pipeline_backend_default.cc',
        'cma/backend/media_pipeline_backend_default.h',
        'cma/backend/video_pipeline_device_default.cc',
        'cma/backend/video_pipeline_device_default.h',
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
      'target_name': 'cma_pipeline',
      'type': '<(component)',
      'dependencies': [
        'cma_base',
        'media_base',
        'media_cdm',
        '../../base/base.gyp:base',
        '../../crypto/crypto.gyp:crypto',
        '../../media/media.gyp:media',
        '../../third_party/boringssl/boringssl.gyp:boringssl',
      ],
      'sources': [
        'cma/pipeline/audio_pipeline.cc',
        'cma/pipeline/audio_pipeline.h',
        'cma/pipeline/audio_pipeline_impl.cc',
        'cma/pipeline/audio_pipeline_impl.h',
        'cma/pipeline/av_pipeline_client.cc',
        'cma/pipeline/av_pipeline_client.h',
        'cma/pipeline/av_pipeline_impl.cc',
        'cma/pipeline/av_pipeline_impl.h',
        'cma/pipeline/decrypt_util.cc',
        'cma/pipeline/decrypt_util.h',
        'cma/pipeline/frame_status_cb_impl.cc',
        'cma/pipeline/frame_status_cb_impl.h',
        'cma/pipeline/load_type.h',
        'cma/pipeline/media_component_device_client_impl.cc',
        'cma/pipeline/media_component_device_client_impl.h',
        'cma/pipeline/media_pipeline.h',
        'cma/pipeline/media_pipeline_client.cc',
        'cma/pipeline/media_pipeline_client.h',
        'cma/pipeline/media_pipeline_impl.cc',
        'cma/pipeline/media_pipeline_impl.h',
        'cma/pipeline/video_pipeline.cc',
        'cma/pipeline/video_pipeline.h',
        'cma/pipeline/video_pipeline_client.cc',
        'cma/pipeline/video_pipeline_client.h',
        'cma/pipeline/video_pipeline_device_client_impl.cc',
        'cma/pipeline/video_pipeline_device_client_impl.h',
        'cma/pipeline/video_pipeline_impl.cc',
        'cma/pipeline/video_pipeline_impl.h',
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
        'cma/filters/cma_renderer.cc',
        'cma/filters/cma_renderer.h',
        'cma/filters/demuxer_stream_adapter.cc',
        'cma/filters/demuxer_stream_adapter.h',
        'cma/filters/hole_frame_factory.cc',
        'cma/filters/hole_frame_factory.h',
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
        'cma_pipeline',
        'default_cma_backend',
        'media_cdm',
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
        '../../chromecast/chromecast.gyp:cast_metrics_test_support',
        '../../gpu/gpu.gyp:gpu_unittest_utils',
        '../../media/media.gyp:media_test_support',
        '../../testing/gmock.gyp:gmock',
        '../../testing/gtest.gyp:gtest',
        '../../testing/gtest.gyp:gtest_main',
        '../../ui/gfx/gfx.gyp:gfx_test_support',
      ],
      'sources': [
        'cdm/chromecast_init_data_unittest.cc',
        'cma/backend/audio_video_pipeline_device_unittest.cc',
        'cma/base/balanced_media_task_runner_unittest.cc',
        'cma/base/buffering_controller_unittest.cc',
        'cma/base/buffering_frame_provider_unittest.cc',
        'cma/filters/demuxer_stream_adapter_unittest.cc',
        'cma/filters/multi_demuxer_stream_adapter_unittest.cc',
        'cma/ipc/media_message_fifo_unittest.cc',
        'cma/ipc/media_message_unittest.cc',
        'cma/ipc_streamer/av_streamer_unittest.cc',
        'cma/pipeline/audio_video_pipeline_impl_unittest.cc',
        'cma/test/cma_end_to_end_test.cc',
        'cma/test/demuxer_stream_for_test.cc',
        'cma/test/demuxer_stream_for_test.h',
        'cma/test/frame_generator_for_test.cc',
        'cma/test/frame_generator_for_test.h',
        'cma/test/frame_segmenter_for_test.cc',
        'cma/test/frame_segmenter_for_test.h',
        'cma/test/media_component_device_feeder_for_test.cc',
        'cma/test/media_component_device_feeder_for_test.h',
        'cma/test/mock_frame_consumer.cc',
        'cma/test/mock_frame_consumer.h',
        'cma/test/mock_frame_provider.cc',
        'cma/test/mock_frame_provider.h',
        'cma/test/run_all_unittests.cc',
      ],
    },
  ], # end of targets
  'conditions': [
    ['use_default_libcast_media==1', {
      'targets': [
        {
          'target_name': 'libcast_media_1.0',
          'type': 'shared_library',
          'dependencies': [
            '../../chromecast/chromecast.gyp:cast_public_api',
            'default_cma_backend'
          ],
          'include_dirs': [
            '../..',
          ],
          'sources': [
            'base/cast_media_default.cc',
          ],
        }
      ]
    }],
  ],
}
