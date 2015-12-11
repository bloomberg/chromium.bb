# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../third_party/mojo/mojo_variables.gypi',
  ],
  'target_defaults' : {
    'include_dirs': [
      '..',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        '..',
      ],
    },
  },
  'targets': [
    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl2',
      'type': 'static_library',
      # TODO(use_chrome_edk): this should be a component to match third_party,
      # but since third_party includes it, we either make it a static library
      # or we have to change the export macros to be different than third_party.
      #'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'defines': [
        'MOJO_SYSTEM_IMPL_IMPLEMENTATION',
        'MOJO_SYSTEM_IMPLEMENTATION',
        'MOJO_USE_SYSTEM_IMPL',
      ],
      'sources': [
        'edk/embedder/configuration.h',
        'edk/embedder/embedder.cc',
        'edk/embedder/embedder.h',
        'edk/embedder/embedder_internal.h',
        'edk/embedder/entrypoints.cc',
        'edk/embedder/platform_channel_pair.cc',
        'edk/embedder/platform_channel_pair.h',
        'edk/embedder/platform_channel_pair_posix.cc',
        'edk/embedder/platform_channel_pair_win.cc',
        'edk/embedder/platform_channel_utils_posix.cc',
        'edk/embedder/platform_channel_utils_posix.h',
        'edk/embedder/platform_handle.cc',
        'edk/embedder/platform_handle.h',
        'edk/embedder/platform_handle_utils.h',
        'edk/embedder/platform_handle_utils_posix.cc',
        'edk/embedder/platform_handle_utils_win.cc',
        'edk/embedder/platform_handle_vector.h',
        'edk/embedder/platform_shared_buffer.h',
        'edk/embedder/platform_support.h',
        'edk/embedder/scoped_platform_handle.h',
        'edk/embedder/simple_platform_shared_buffer.cc',
        'edk/embedder/simple_platform_shared_buffer.h',
        'edk/embedder/simple_platform_shared_buffer_android.cc',
        'edk/embedder/simple_platform_shared_buffer_posix.cc',
        'edk/embedder/simple_platform_shared_buffer_win.cc',
        'edk/embedder/simple_platform_support.cc',
        'edk/embedder/simple_platform_support.h',
        'edk/system/awakable.h',
        'edk/system/awakable_list.cc',
        'edk/system/awakable_list.h',
        'edk/system/async_waiter.cc',
        'edk/system/async_waiter.h',
        'edk/system/broker.h',
        'edk/system/broker_messages.h',
        'edk/system/broker_state.cc',
        'edk/system/broker_state.h',
        'edk/system/child_broker_host.cc',
        'edk/system/child_broker_host.h',
        'edk/system/configuration.cc',
        'edk/system/configuration.h',
        'edk/system/core.cc',
        'edk/system/core.h',
        'edk/system/data_pipe.cc',
        'edk/system/data_pipe.h',
        'edk/system/data_pipe_consumer_dispatcher.cc',
        'edk/system/data_pipe_consumer_dispatcher.h',
        'edk/system/data_pipe_producer_dispatcher.cc',
        'edk/system/data_pipe_producer_dispatcher.h',
        'edk/system/dispatcher.cc',
        'edk/system/dispatcher.h',
        'edk/system/handle_signals_state.h',
        'edk/system/handle_table.cc',
        'edk/system/handle_table.h',
        'edk/system/mapping_table.cc',
        'edk/system/mapping_table.h',
        'edk/system/message_in_transit.cc',
        'edk/system/message_in_transit.h',
        'edk/system/message_in_transit_queue.cc',
        'edk/system/message_in_transit_queue.h',
        'edk/system/message_pipe_dispatcher.cc',
        'edk/system/message_pipe_dispatcher.h',
        'edk/system/options_validation.h',
        'edk/system/child_broker.cc',
        'edk/system/child_broker.h',
        'edk/system/platform_handle_dispatcher.cc',
        'edk/system/platform_handle_dispatcher.h',
        'edk/system/raw_channel.cc',
        'edk/system/raw_channel.h',
        'edk/system/raw_channel_posix.cc',
        'edk/system/raw_channel_win.cc',
        'edk/system/routed_raw_channel.cc',
        'edk/system/routed_raw_channel.h',
        'edk/system/shared_buffer_dispatcher.cc',
        'edk/system/shared_buffer_dispatcher.h',
        'edk/system/simple_dispatcher.cc',
        'edk/system/simple_dispatcher.h',
        'edk/system/transport_data.cc',
        'edk/system/transport_data.h',
        'edk/system/wait_set_dispatcher.cc',
        'edk/system/wait_set_dispatcher.h',
        'edk/system/waiter.cc',
        'edk/system/waiter.h',
        # Test-only code:
        # TODO(vtl): It's a little unfortunate that these end up in the same
        # component as non-test-only code. In the static build, this code
        # should hopefully be dead-stripped.
        'edk/embedder/test_embedder.cc',
        'edk/embedder/test_embedder.h',
      ],
      'all_dependent_settings': {
        # Ensures that dependent projects import the core functions on Windows.
        'defines': ['MOJO_USE_SYSTEM_IMPL'],
      },
      'conditions': [
        ['OS=="android"', {
          'dependencies': [
            '../third_party/ashmem/ashmem.gyp:ashmem',
          ],
        }],
        ['OS=="win"', {
           # Structure was padded due to __declspec(align()), which is
           # uninteresting.
          'msvs_disabled_warnings': [ 4324 ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/js
      # TODO(use_chrome_edk): remove "2"
      'target_name': 'mojo_js_lib2',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
      ],
      'sources': [
        # Sources list duplicated in GN build.
        'edk/js/core.cc',
        'edk/js/core.h',
        'edk/js/drain_data.cc',
        'edk/js/drain_data.h',
        'edk/js/handle.cc',
        'edk/js/handle.h',
        'edk/js/handle_close_observer.h',
        'edk/js/mojo_runner_delegate.cc',
        'edk/js/mojo_runner_delegate.h',
        'edk/js/support.cc',
        'edk/js/support.h',
        'edk/js/threading.cc',
        'edk/js/threading.h',
        'edk/js/waiting_callback.cc',
        'edk/js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support_impl
      # TODO(use_chrome_edk): remove "2"
      'target_name': 'mojo_test_support_impl2',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'edk/test/test_support_impl.cc',
        'edk/test/test_support_impl.h',
      ],
    },
    {
      # GN version: //mojo/edk/test:test_support
      'target_name': 'mojo_common_test_support2',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system_impl2',
      ],
      'sources': [
        'edk/test/multiprocess_test_helper.cc',
        'edk/test/multiprocess_test_helper.h',
        'edk/test/scoped_ipc_support.cc',
        'edk/test/scoped_ipc_support.h',
        'edk/test/test_utils.h',
        'edk/test/test_utils_posix.cc',
        'edk/test/test_utils_win.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/test/multiprocess_test_helper.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_unittests
      # TODO(use_chrome_edk): remove "2"
      'target_name': 'mojo_run_all_unittests2',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../third_party/mojo/mojo_public.gyp:mojo_public_test_support',
        'mojo_system_impl2',
        'mojo_test_support_impl2',
      ],
      'sources': [
        'edk/test/run_all_unittests.cc',
      ],
    },
  ],
}
