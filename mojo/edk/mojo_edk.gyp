# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl',
      'type': '<(component)',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPL_IMPLEMENTATION',
        'MOJO_SYSTEM_IMPLEMENTATION',
        'MOJO_USE_SYSTEM_IMPL',
      ],
      'sources': [
        'embedder/configuration.h',
        'embedder/channel_info_forward.h',
        'embedder/channel_init.cc',
        'embedder/channel_init.h',
        'embedder/embedder.cc',
        'embedder/embedder.h',
        'embedder/embedder_internal.h',
        'embedder/entrypoints.cc',
        'embedder/platform_channel_pair.cc',
        'embedder/platform_channel_pair.h',
        'embedder/platform_channel_pair_posix.cc',
        'embedder/platform_channel_pair_win.cc',
        'embedder/platform_channel_utils_posix.cc',
        'embedder/platform_channel_utils_posix.h',
        'embedder/platform_handle.cc',
        'embedder/platform_handle.h',
        'embedder/platform_handle_utils.h',
        'embedder/platform_handle_utils_posix.cc',
        'embedder/platform_handle_utils_win.cc',
        'embedder/platform_handle_vector.h',
        'embedder/platform_shared_buffer.h',
        'embedder/platform_support.h',
        'embedder/scoped_platform_handle.h',
        'embedder/simple_platform_shared_buffer.cc',
        'embedder/simple_platform_shared_buffer.h',
        'embedder/simple_platform_shared_buffer_posix.cc',
        'embedder/simple_platform_shared_buffer_win.cc',
        'embedder/simple_platform_support.cc',
        'embedder/simple_platform_support.h',
        'system/channel.cc',
        'system/channel.h',
        'system/channel_endpoint.cc',
        'system/channel_endpoint.h',
        'system/channel_endpoint_id.cc',
        'system/channel_endpoint_id.h',
        'system/channel_info.cc',
        'system/channel_info.h',
        'system/channel_manager.cc',
        'system/channel_manager.h',
        'system/configuration.cc',
        'system/configuration.h',
        'system/core.cc',
        'system/core.h',
        'system/data_pipe.cc',
        'system/data_pipe.h',
        'system/data_pipe_consumer_dispatcher.cc',
        'system/data_pipe_consumer_dispatcher.h',
        'system/data_pipe_producer_dispatcher.cc',
        'system/data_pipe_producer_dispatcher.h',
        'system/dispatcher.cc',
        'system/dispatcher.h',
        'system/handle_signals_state.h',
        'system/handle_table.cc',
        'system/handle_table.h',
        'system/local_data_pipe.cc',
        'system/local_data_pipe.h',
        'system/local_message_pipe_endpoint.cc',
        'system/local_message_pipe_endpoint.h',
        'system/mapping_table.cc',
        'system/mapping_table.h',
        'system/memory.cc',
        'system/memory.h',
        'system/message_in_transit.cc',
        'system/message_in_transit.h',
        'system/message_in_transit_queue.cc',
        'system/message_in_transit_queue.h',
        'system/message_pipe.cc',
        'system/message_pipe.h',
        'system/message_pipe_dispatcher.cc',
        'system/message_pipe_dispatcher.h',
        'system/message_pipe_endpoint.cc',
        'system/message_pipe_endpoint.h',
        'system/options_validation.h',
        'system/platform_handle_dispatcher.cc',
        'system/platform_handle_dispatcher.h',
        'system/proxy_message_pipe_endpoint.cc',
        'system/proxy_message_pipe_endpoint.h',
        'system/raw_channel.cc',
        'system/raw_channel.h',
        'system/raw_channel_posix.cc',
        'system/raw_channel_win.cc',
        'system/shared_buffer_dispatcher.cc',
        'system/shared_buffer_dispatcher.h',
        'system/simple_dispatcher.cc',
        'system/simple_dispatcher.h',
        'system/transport_data.cc',
        'system/transport_data.h',
        'system/waiter.cc',
        'system/waiter.h',
        'system/waiter_list.cc',
        'system/waiter_list.h',
        # Test-only code:
        # TODO(vtl): It's a little unfortunate that these end up in the same
        # component as non-test-only code. In the static build, this code should
        # hopefully be dead-stripped.
        'embedder/test_embedder.cc',
        'embedder/test_embedder.h',
      ],
      'all_dependent_settings': {
        # Ensures that dependent projects import the core functions on Windows.
        'defines': ['MOJO_USE_SYSTEM_IMPL'],
      }
    },
    {
      # GN version: //mojo/edk/js
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../gin/gin.gyp:gin',
        '../../v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../../base/base.gyp:base',
        '../../gin/gin.gyp:gin',
      ],
      'sources': [
        # Sources list duplicated in GN build.
        'js/core.cc',
        'js/core.h',
        'js/drain_data.cc',
        'js/drain_data.h',
        'js/handle.cc',
        'js/handle.h',
        'js/handle_close_observer.h',
        'js/mojo_runner_delegate.cc',
        'js/mojo_runner_delegate.h',
        'js/support.cc',
        'js/support.h',
        'js/threading.cc',
        'js/threading.h',
        'js/waiting_callback.cc',
        'js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support_impl
      'target_name': 'mojo_test_support_impl',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'test/test_support_impl.cc',
        'test/test_support_impl.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support
      'target_name': 'mojo_common_test_support',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'mojo_system_impl',
      ],
      'sources': [
        'test/multiprocess_test_helper.cc',
        'test/multiprocess_test_helper.h',
        'test/test_utils.h',
        'test/test_utils_posix.cc',
        'test/test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'test/multiprocess_test_helper.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_unittests
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../testing/gtest.gyp:gtest',
        'mojo_system_impl',
        'mojo_test_support_impl',
        '../public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'test/run_all_unittests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_perftests
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:test_support_base',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_test_support_impl',
        '../public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'test/run_all_perftests.cc',
      ],
    },
  ],
}
