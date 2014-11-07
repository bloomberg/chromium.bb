# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Essential components (and their tests) that are needed to build
# Chrome should be here.  Other components that are useful only in
# Mojo land like mojo_shell should be in mojo.gyp.
{
  'includes': [
    '../mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_edk',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_message_pipe_perftests',
        'mojo_public_application_unittests',
        'mojo_public_bindings_unittests',
        'mojo_public_environment_unittests',
        'mojo_public_system_perftests',
        'mojo_public_system_unittests',
        'mojo_public_utility_unittests',
        'mojo_system_impl',
        'mojo_system_unittests',
        'mojo_js_unittests',
      ],
    },
    {
      'target_name': 'mojo_none',
      'type': 'none',
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
        'mojo_system_impl',
        'mojo_test_support_impl',
        '../public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'test/run_all_perftests.cc',
      ],
    },
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      # GN version: //mojo/public/cpp/bindings/tests:mojo_public_bindings_unittests
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        '../public/mojo_public.gyp:mojo_cpp_bindings',
        '../public/mojo_public.gyp:mojo_environment_standalone',
        '../public/mojo_public.gyp:mojo_public_bindings_test_utils',
        '../public/mojo_public.gyp:mojo_public_test_interfaces',
        '../public/mojo_public.gyp:mojo_public_test_utils',
        '../public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        '../public/cpp/bindings/tests/array_unittest.cc',
        '../public/cpp/bindings/tests/bounds_checker_unittest.cc',
        '../public/cpp/bindings/tests/buffer_unittest.cc',
        '../public/cpp/bindings/tests/connector_unittest.cc',
        '../public/cpp/bindings/tests/container_test_util.cc',
        '../public/cpp/bindings/tests/equals_unittest.cc',
        '../public/cpp/bindings/tests/handle_passing_unittest.cc',
        '../public/cpp/bindings/tests/interface_ptr_unittest.cc',
        '../public/cpp/bindings/tests/map_unittest.cc',
        '../public/cpp/bindings/tests/request_response_unittest.cc',
        '../public/cpp/bindings/tests/router_unittest.cc',
        '../public/cpp/bindings/tests/sample_service_unittest.cc',
        '../public/cpp/bindings/tests/serialization_warning_unittest.cc',
        '../public/cpp/bindings/tests/string_unittest.cc',
        '../public/cpp/bindings/tests/struct_unittest.cc',
        '../public/cpp/bindings/tests/type_conversion_unittest.cc',
        '../public/cpp/bindings/tests/validation_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment/tests:mojo_public_environment_unittests
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        '../public/mojo_public.gyp:mojo_environment_standalone',
        '../public/mojo_public.gyp:mojo_public_test_utils',
        '../public/mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '../..' ],
      'sources': [
        '../public/cpp/environment/tests/async_wait_unittest.cc',
        '../public/cpp/environment/tests/async_waiter_unittest.cc',
        '../public/cpp/environment/tests/logger_unittest.cc',
        '../public/cpp/environment/tests/logging_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_application_unittests
      'target_name': 'mojo_public_application_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        '../public/mojo_public.gyp:mojo_application_standalone',
        '../public/mojo_public.gyp:mojo_utility',
        '../public/mojo_public.gyp:mojo_environment_standalone',
      ],
      'sources': [
        '../public/cpp/application/tests/service_registry_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_system_unittests
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        '../public/mojo_public.gyp:mojo_public_test_utils',
      ],
      'include_dirs': [ '../..' ],
      'sources': [
        '<@(mojo_public_system_unittest_sources)',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_utility_unittests
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        '../public/mojo_public.gyp:mojo_public_test_utils',
        '../public/mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '../..' ],
      'sources': [
        '../public/cpp/utility/tests/mutex_unittest.cc',
        '../public/cpp/utility/tests/run_loop_unittest.cc',
        '../public/cpp/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            '../public/cpp/utility/tests/mutex_unittest.cc',
            '../public/cpp/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/public/c/system/tests:perftests
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_run_all_perftests',
        '../public/mojo_public.gyp:mojo_public_test_utils',
        '../public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        '../public/c/system/tests/core_perftest.cc',
      ],
    },

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
        'embedder/channel_info_forward.h',
        'embedder/channel_init.cc',
        'embedder/channel_init.h',
        'embedder/embedder.cc',
        'embedder/embedder.h',
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
        'system/constants.h',
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
        'system/entrypoints.cc',
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
      # GN version: //mojo/edk/system:mojo_system_unittests
      'target_name': 'mojo_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_system_impl',
      ],
      'sources': [
        'embedder/embedder_unittest.cc',
        'embedder/platform_channel_pair_posix_unittest.cc',
        'embedder/simple_platform_shared_buffer_unittest.cc',
        'system/channel_endpoint_id_unittest.cc',
        'system/channel_unittest.cc',
        'system/core_unittest.cc',
        'system/core_test_base.cc',
        'system/core_test_base.h',
        'system/data_pipe_unittest.cc',
        'system/dispatcher_unittest.cc',
        'system/local_data_pipe_unittest.cc',
        'system/memory_unittest.cc',
        'system/message_pipe_dispatcher_unittest.cc',
        'system/message_pipe_test_utils.h',
        'system/message_pipe_test_utils.cc',
        'system/message_pipe_unittest.cc',
        'system/multiprocess_message_pipe_unittest.cc',
        'system/options_validation_unittest.cc',
        'system/platform_handle_dispatcher_unittest.cc',
        'system/raw_channel_unittest.cc',
        'system/remote_message_pipe_unittest.cc',
        'system/run_all_unittests.cc',
        'system/shared_buffer_dispatcher_unittest.cc',
        'system/simple_dispatcher_unittest.cc',
        'system/test_utils.cc',
        'system/test_utils.h',
        'system/waiter_list_unittest.cc',
        'system/waiter_test_utils.cc',
        'system/waiter_test_utils.h',
        'system/waiter_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'embedder/embedder_unittest.cc',
            'system/multiprocess_message_pipe_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_message_pipe_perftests
      'target_name': 'mojo_message_pipe_perftests',
      'type': 'executable',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/base.gyp:test_support_base',
        '../../base/base.gyp:test_support_perf',
        '../../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_system_impl',
      ],
      'sources': [
        'system/message_pipe_perftest.cc',
        'system/message_pipe_test_utils.h',
        'system/message_pipe_test_utils.cc',
        'system/test_utils.cc',
        'system/test_utils.h',
      ],
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
        'js/support.cc',
        'js/support.h',
        'js/waiting_callback.cc',
        'js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/edk/js:js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../../gin/gin.gyp:gin_test',
        'mojo_common_test_support',
        'mojo_run_all_unittests',
        'mojo_js_lib',
        '../public/mojo_public.gyp:mojo_environment_standalone',
        '../public/mojo_public.gyp:mojo_public_test_interfaces',
        '../public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'js/handle_unittest.cc',
        'js/tests/run_js_tests.cc',
      ],
    },
    {
      # GN version: //mojo/common/test:test_support_impl
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
  ],
}
