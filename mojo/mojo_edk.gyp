# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
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
      'target_name': 'mojo_system_impl',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../crypto/crypto.gyp:crypto',
        'mojo_public.gyp:mojo_system_headers',
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
        'edk/embedder/platform_shared_buffer.cc',
        'edk/embedder/platform_shared_buffer.h',
        'edk/embedder/scoped_platform_handle.h',
        'edk/system/awakable.h',
        'edk/system/awakable_list.cc',
        'edk/system/awakable_list.h',
        'edk/system/async_waiter.cc',
        'edk/system/async_waiter.h',
        'edk/system/atomic_flag.h',
        'edk/system/broker.h',
        'edk/system/broker_host.h',
        'edk/system/broker_host_posix.cc',
        'edk/system/broker_posix.cc',
        'edk/system/channel.cc',
        'edk/system/channel.h',
        'edk/system/channel_posix.cc',
        'edk/system/channel_win.cc',
        'edk/system/configuration.cc',
        'edk/system/configuration.h',
        'edk/system/core.cc',
        'edk/system/core.h',
        'edk/system/data_pipe_consumer_dispatcher.cc',
        'edk/system/data_pipe_consumer_dispatcher.h',
        'edk/system/data_pipe_control_message.cc',
        'edk/system/data_pipe_control_message.h',
        'edk/system/data_pipe_producer_dispatcher.cc',
        'edk/system/data_pipe_producer_dispatcher.h',
        'edk/system/dispatcher.cc',
        'edk/system/dispatcher.h',
        'edk/system/handle_signals_state.h',
        'edk/system/handle_table.cc',
        'edk/system/handle_table.h',
        'edk/system/mapping_table.cc',
        'edk/system/mapping_table.h',
        'edk/system/message_for_transit.cc',
        'edk/system/message_for_transit.h',
        'edk/system/message_pipe_dispatcher.cc',
        'edk/system/message_pipe_dispatcher.h',
        'edk/system/node_channel.cc',
        'edk/system/node_channel.h',
        'edk/system/node_controller.cc',
        'edk/system/node_controller.h',
        'edk/system/options_validation.h',
        'edk/system/platform_handle_dispatcher.cc',
        'edk/system/platform_handle_dispatcher.h',
        'edk/system/ports/event.cc',
        'edk/system/ports/event.h',
        'edk/system/ports/hash_functions.h',
        'edk/system/ports/message.cc',
        'edk/system/ports/message.h',
        'edk/system/ports/message_queue.cc',
        'edk/system/ports/message_queue.h',
        'edk/system/ports/name.cc',
        'edk/system/ports/name.h',
        'edk/system/ports/node.cc',
        'edk/system/ports/node.h',
        'edk/system/ports/node_delegate.h',
        'edk/system/ports/port.cc',
        'edk/system/ports/port.h',
        'edk/system/ports/port_ref.cc',
        'edk/system/ports/user_data.h',
        'edk/system/ports_message.cc',
        'edk/system/ports_message.h',
        'edk/system/remote_message_pipe_bootstrap.cc',
        'edk/system/remote_message_pipe_bootstrap.h',
        'edk/system/request_context.cc',
        'edk/system/request_context.h',
        'edk/system/shared_buffer_dispatcher.cc',
        'edk/system/shared_buffer_dispatcher.h',
        'edk/system/wait_set_dispatcher.cc',
        'edk/system/wait_set_dispatcher.h',
        'edk/system/waiter.cc',
        'edk/system/waiter.h',
        'edk/system/watcher.cc',
        'edk/system/watcher.h',
        'edk/system/watcher_set.cc',
        'edk/system/watcher_set.h',
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
        ['OS=="mac" and OS!="ios"', {
          'sources': [
            'edk/system/mach_port_relay.cc',
            'edk/system/mach_port_relay.h',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/js
      'target_name': 'mojo_js_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        '../v8/src/v8.gyp:v8',
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
      'target_name': 'mojo_test_support_impl',
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
      'target_name': 'mojo_common_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system_impl',
      ],
      'sources': [
        'edk/test/mojo_test_base.cc',
        'edk/test/mojo_test_base.h',
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
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_public.gyp:mojo_public_test_support',
        'mojo_system_impl',
        'mojo_test_support_impl',
      ],
      'sources': [
        'edk/test/run_all_unittests.cc',
      ],
    },
    {
      # GN version: //mojo/edk/test:run_all_perftests
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_public.gyp:mojo_public_test_support',
        'mojo_system_impl',
        'mojo_test_support_impl',
      ],
      'sources': [
        'edk/test/run_all_perftests.cc',
      ],
    },
  ],
}
