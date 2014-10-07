# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Essential components (and their tests) that are needed to build
# Chrome should be here.  Other components that are useful only in
# Mojo land like mojo_shell should be in mojo.gyp.
{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_base',
      'type': 'none',
      'dependencies': [
        # NOTE: If adding a new dependency here, please consider whether it
        # should also be added to the list of Mojo-related dependencies of
        # build/all.gyp:All on iOS, as All cannot depend on the mojo_base
        # target on iOS due to the presence of the js targets, which cause v8
        # to be built.
        'mojo_common_lib',
        'mojo_common_unittests',
        'mojo_message_generator',
        'mojo_message_pipe_perftests',
        'mojo_public_application_unittests',
        'mojo_public_bindings_unittests',
        'mojo_public_environment_unittests',
        'mojo_public_system_perftests',
        'mojo_public_system_unittests',
        'mojo_public_utility_unittests',
        'mojo_system_impl',
        'mojo_system_unittests',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'libmojo_system_java',
            'mojo_system_java',
            'mojo_test_apk',
            'public/mojo_public.gyp:mojo_bindings_java',
            'public/mojo_public.gyp:mojo_public_java',
          ],
        }],
      ]
    },
    {
      'target_name': 'mojo_none',
      'type': 'none',
    },
    {
      # GN version: //mojo/common/test:run_all_unittests
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system_impl',
        'mojo_test_support_impl',
        'public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'common/test/run_all_unittests.cc',
      ],
    },
    {
      # GN version: //mojo/common/test:run_all_perftests
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        'mojo_system_impl',
        'mojo_test_support_impl',
        'public/mojo_public.gyp:mojo_test_support',
      ],
      'sources': [
        'common/test/run_all_perftests.cc',
      ],
    },
    # TODO(vtl): Reorganize the mojo_public_*_unittests.
    {
      # GN version: //mojo/public/cpp/bindings/tests:mojo_public_bindings_unittests
      'target_name': 'mojo_public_bindings_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        'public/mojo_public.gyp:mojo_cpp_bindings',
        'public/mojo_public.gyp:mojo_environment_standalone',
        'public/mojo_public.gyp:mojo_public_bindings_test_utils',
        'public/mojo_public.gyp:mojo_public_test_interfaces',
        'public/mojo_public.gyp:mojo_public_test_utils',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'public/cpp/bindings/tests/array_unittest.cc',
        'public/cpp/bindings/tests/bounds_checker_unittest.cc',
        'public/cpp/bindings/tests/buffer_unittest.cc',
        'public/cpp/bindings/tests/connector_unittest.cc',
        'public/cpp/bindings/tests/handle_passing_unittest.cc',
        'public/cpp/bindings/tests/interface_ptr_unittest.cc',
        'public/cpp/bindings/tests/request_response_unittest.cc',
        'public/cpp/bindings/tests/router_unittest.cc',
        'public/cpp/bindings/tests/sample_service_unittest.cc',
        'public/cpp/bindings/tests/serialization_warning_unittest.cc',
        'public/cpp/bindings/tests/string_unittest.cc',
        'public/cpp/bindings/tests/struct_unittest.cc',
        'public/cpp/bindings/tests/type_conversion_unittest.cc',
        'public/cpp/bindings/tests/validation_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/environment/tests:mojo_public_environment_unittests
      'target_name': 'mojo_public_environment_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        'public/mojo_public.gyp:mojo_environment_standalone',
        'public/mojo_public.gyp:mojo_public_test_utils',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'include_dirs': [ '..' ],
      'sources': [
        'public/cpp/environment/tests/async_waiter_unittest.cc',
        'public/cpp/environment/tests/logger_unittest.cc',
        'public/cpp/environment/tests/logging_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_application_unittests
      'target_name': 'mojo_public_application_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_run_all_unittests',
        'public/mojo_public.gyp:mojo_application_standalone',
        'public/mojo_public.gyp:mojo_utility',
        'public/mojo_public.gyp:mojo_environment_standalone',
      ],
      'sources': [
        'public/cpp/application/tests/service_registry_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_system_unittests
      'target_name': 'mojo_public_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_base.gyp:mojo_run_all_unittests',
        'public/mojo_public.gyp:mojo_public_test_utils',
      ],
      'include_dirs': [ '..' ],
      'sources': [
        '<@(mojo_public_system_unittest_sources)',
      ],
    },
    {
      # GN version: //mojo/public/cpp/application/tests:mojo_public_utility_unittests
      'target_name': 'mojo_public_utility_unittests',
      'type': 'executable',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        'mojo_base.gyp:mojo_run_all_unittests',
        'public/mojo_public.gyp:mojo_public_test_utils',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'include_dirs' : [ '..' ],
      'sources': [
        'public/cpp/utility/tests/mutex_unittest.cc',
        'public/cpp/utility/tests/run_loop_unittest.cc',
        'public/cpp/utility/tests/thread_unittest.cc',
      ],
      'conditions': [
        # See crbug.com/342893:
        ['OS=="win"', {
          'sources!': [
            'public/cpp/utility/tests/mutex_unittest.cc',
            'public/cpp/utility/tests/thread_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/public/c/system/tests:perftests
      'target_name': 'mojo_public_system_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_run_all_perftests',
        'public/mojo_public.gyp:mojo_public_test_utils',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'public/c/system/tests/core_perftest.cc',
      ],
    },

    {
      # GN version: //mojo/edk/system
      'target_name': 'mojo_system_impl',
      'type': '<(component)',
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
        'edk/embedder/channel_init.cc',
        'edk/embedder/channel_init.h',
        'edk/embedder/embedder.cc',
        'edk/embedder/embedder.h',
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
        'edk/embedder/simple_platform_shared_buffer_posix.cc',
        'edk/embedder/simple_platform_shared_buffer_win.cc',
        'edk/embedder/simple_platform_support.cc',
        'edk/embedder/simple_platform_support.h',
        'edk/system/channel.cc',
        'edk/system/channel.h',
        'edk/system/channel_endpoint.cc',
        'edk/system/channel_endpoint.h',
        'edk/system/constants.h',
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
        'edk/system/entrypoints.cc',
        'edk/system/handle_signals_state.h',
        'edk/system/handle_table.cc',
        'edk/system/handle_table.h',
        'edk/system/local_data_pipe.cc',
        'edk/system/local_data_pipe.h',
        'edk/system/local_message_pipe_endpoint.cc',
        'edk/system/local_message_pipe_endpoint.h',
        'edk/system/mapping_table.cc',
        'edk/system/mapping_table.h',
        'edk/system/memory.cc',
        'edk/system/memory.h',
        'edk/system/message_in_transit.cc',
        'edk/system/message_in_transit.h',
        'edk/system/message_in_transit_queue.cc',
        'edk/system/message_in_transit_queue.h',
        'edk/system/message_pipe.cc',
        'edk/system/message_pipe.h',
        'edk/system/message_pipe_dispatcher.cc',
        'edk/system/message_pipe_dispatcher.h',
        'edk/system/message_pipe_endpoint.cc',
        'edk/system/message_pipe_endpoint.h',
        'edk/system/options_validation.h',
        'edk/system/platform_handle_dispatcher.cc',
        'edk/system/platform_handle_dispatcher.h',
        'edk/system/proxy_message_pipe_endpoint.cc',
        'edk/system/proxy_message_pipe_endpoint.h',
        'edk/system/raw_channel.cc',
        'edk/system/raw_channel.h',
        'edk/system/raw_channel_posix.cc',
        'edk/system/raw_channel_win.cc',
        'edk/system/shared_buffer_dispatcher.cc',
        'edk/system/shared_buffer_dispatcher.h',
        'edk/system/simple_dispatcher.cc',
        'edk/system/simple_dispatcher.h',
        'edk/system/transport_data.cc',
        'edk/system/transport_data.h',
        'edk/system/waiter.cc',
        'edk/system/waiter.h',
        'edk/system/waiter_list.cc',
        'edk/system/waiter_list.h',
        # Test-only code:
        # TODO(vtl): It's a little unfortunate that these end up in the same
        # component as non-test-only code. In the static build, this code should
        # hopefully be dead-stripped.
        'edk/embedder/test_embedder.cc',
        'edk/embedder/test_embedder.h',
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
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_system_impl',
      ],
      'sources': [
        'edk/embedder/embedder_unittest.cc',
        'edk/embedder/platform_channel_pair_posix_unittest.cc',
        'edk/embedder/simple_platform_shared_buffer_unittest.cc',
        'edk/system/channel_unittest.cc',
        'edk/system/core_unittest.cc',
        'edk/system/core_test_base.cc',
        'edk/system/core_test_base.h',
        'edk/system/data_pipe_unittest.cc',
        'edk/system/dispatcher_unittest.cc',
        'edk/system/local_data_pipe_unittest.cc',
        'edk/system/memory_unittest.cc',
        'edk/system/message_pipe_dispatcher_unittest.cc',
        'edk/system/message_pipe_test_utils.h',
        'edk/system/message_pipe_test_utils.cc',
        'edk/system/message_pipe_unittest.cc',
        'edk/system/multiprocess_message_pipe_unittest.cc',
        'edk/system/options_validation_unittest.cc',
        'edk/system/platform_handle_dispatcher_unittest.cc',
        'edk/system/raw_channel_unittest.cc',
        'edk/system/remote_message_pipe_unittest.cc',
        'edk/system/run_all_unittests.cc',
        'edk/system/shared_buffer_dispatcher_unittest.cc',
        'edk/system/simple_dispatcher_unittest.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
        'edk/system/waiter_list_unittest.cc',
        'edk/system/waiter_test_utils.cc',
        'edk/system/waiter_test_utils.h',
        'edk/system/waiter_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/embedder/embedder_unittest.cc',
            'edk/system/multiprocess_message_pipe_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/edk/system:mojo_message_pipe_perftests
      'target_name': 'mojo_message_pipe_perftests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/base.gyp:test_support_perf',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_environment_chromium',
        'mojo_system_impl',
      ],
      'sources': [
        'edk/system/message_pipe_perftest.cc',
        'edk/system/message_pipe_test_utils.h',
        'edk/system/message_pipe_test_utils.cc',
        'edk/system/test_utils.cc',
        'edk/system/test_utils.h',
      ],
    },
    {
      # GN version: //mojo/common/test:test_support_impl
      'target_name': 'mojo_test_support_impl',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'common/test/test_support_impl.cc',
        'common/test/test_support_impl.h',
      ],
    },
    {
      # GN version: //mojo/common
      'target_name': 'mojo_common_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_COMMON_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'sources': [
        'common/common_type_converters.cc',
        'common/common_type_converters.h',
        'common/data_pipe_utils.cc',
        'common/data_pipe_utils.h',
        'common/handle_watcher.cc',
        'common/handle_watcher.h',
        'common/message_pump_mojo.cc',
        'common/message_pump_mojo.h',
        'common/message_pump_mojo_handler.h',
        'common/time_helper.cc',
        'common/time_helper.h',
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
        'edk/test/multiprocess_test_helper.cc',
        'edk/test/multiprocess_test_helper.h',
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
      # GN version: //mojo/common:mojo_common_unittests
      'target_name': 'mojo_common_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_message_loop_tests',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'mojo_common_lib',
        'mojo_common_test_support',
        'mojo_environment_chromium',
        'mojo_run_all_unittests',
        'public/mojo_public.gyp:mojo_cpp_bindings',
        'public/mojo_public.gyp:mojo_public_test_utils',
      ],
      'sources': [
        'common/common_type_converters_unittest.cc',
        'common/handle_watcher_unittest.cc',
        'common/message_pump_mojo_unittest.cc',
        'edk/test/multiprocess_test_helper_unittest.cc',
      ],
      'conditions': [
        ['OS=="ios"', {
          'sources!': [
            'edk/test/multiprocess_test_helper_unittest.cc',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/environment:chromium
      'target_name': 'mojo_environment_chromium',
      'type': 'static_library',
      'dependencies': [
        'mojo_environment_chromium_impl',
      ],
      'sources': [
        'environment/environment.cc',
        # TODO(vtl): This is kind of ugly. (See TODO in logging.h.)
        "public/cpp/environment/logging.h",
        "public/cpp/environment/lib/logging.cc",
      ],
      'include_dirs': [
        '..',
      ],
      'export_dependent_settings': [
        'mojo_environment_chromium_impl',
      ],
    },
    {
      # GN version: //mojo/environment:chromium_impl
      'target_name': 'mojo_environment_chromium_impl',
      'type': '<(component)',
      'defines': [
        'MOJO_ENVIRONMENT_IMPL_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'mojo_common_lib',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'environment/default_async_waiter_impl.cc',
        'environment/default_async_waiter_impl.h',
        'environment/default_logger_impl.cc',
        'environment/default_logger_impl.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
     # GN version: //mojo/application
     'target_name': 'mojo_application_chromium',
     'type': 'static_library',
     'sources': [
       'application/application_runner_chromium.cc',
       'application/application_runner_chromium.h',
      ],
      'dependencies': [
        'mojo_common_lib',
        'mojo_environment_chromium',
        'public/mojo_public.gyp:mojo_application_base',
       ],
      'export_dependent_settings': [
        'public/mojo_public.gyp:mojo_application_base',
       ],
    },
    {
      # GN version: //mojo/bindings/js
      'target_name': 'mojo_js_bindings_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        '../v8/tools/gyp/v8.gyp:v8',
        'mojo_common_lib',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../gin/gin.gyp:gin',
        'mojo_common_lib',
      ],
      'sources': [
        # Sources list duplicated in GN build.
        'bindings/js/core.cc',
        'bindings/js/core.h',
        'bindings/js/drain_data.cc',
        'bindings/js/drain_data.h',
        'bindings/js/handle.cc',
        'bindings/js/handle.h',
        'bindings/js/handle_close_observer.h',
        'bindings/js/support.cc',
        'bindings/js/support.h',
        'bindings/js/waiting_callback.cc',
        'bindings/js/waiting_callback.h',
      ],
    },
    {
      # GN version: //mojo/tools:message_generator
      'target_name': 'mojo_message_generator',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_system_impl',
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        'tools/message_generator.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'mojo_jni_headers',
          'type': 'none',
          'dependencies': [
            'mojo_java_set_jni_headers',
          ],
          'sources': [
            'android/javatests/src/org/chromium/mojo/MojoTestCase.java',
            'android/javatests/src/org/chromium/mojo/bindings/ValidationTestUtil.java',
            'android/system/src/org/chromium/mojo/system/impl/CoreImpl.java',
            'services/native_viewport/android/src/org/chromium/mojo/PlatformViewportAndroid.java',
            'shell/android/apk/src/org/chromium/mojo_shell_apk/MojoMain.java',
          ],
          'variables': {
            'jni_gen_package': 'mojo',
         },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'mojo_java_set_jni_headers',
          'type': 'none',
          'variables': {
            'jni_gen_package': 'mojo',
            'input_java_class': 'java/util/HashSet.class',
          },
          'includes': [ '../build/jar_file_jni_generator.gypi' ],
        },
        {
          'target_name': 'mojo_system_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            'public/mojo_public.gyp:mojo_public_java',
          ],
          'variables': {
            'java_in_dir': '<(DEPTH)/mojo/android/system',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'libmojo_system_java',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_jni_headers',
	          'mojo_system_impl',
          ],
          'sources': [
            'android/system/core_impl.cc',
            'android/system/core_impl.h',
          ],
        },
        {
          'target_name': 'libmojo_java_unittest',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            'libmojo_system_java',
            'mojo_jni_headers',
            'public/mojo_public.gyp:mojo_public_bindings_test_utils',
          ],
          'defines': [
            'UNIT_TEST'  # As exported from testing/gtest.gyp:gtest.
          ],
          'sources': [
            'android/javatests/mojo_test_case.cc',
            'android/javatests/mojo_test_case.h',
            'android/javatests/init_library.cc',
            'android/javatests/validation_test_util.cc',
            'android/javatests/validation_test_util.h',
          ],
        },
        {
          'target_name': 'mojo_test_apk',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java_test_support',
            'public/mojo_public.gyp:mojo_bindings_java',
            'mojo_system_java',
            'public/mojo_public.gyp:mojo_public_test_interfaces',
          ],
          'variables': {
            'apk_name': 'MojoTest',
            'java_in_dir': '<(DEPTH)/mojo/android/javatests',
            'resource_dir': '<(DEPTH)/mojo/android/javatests/apk',
            'native_lib_target': 'libmojo_java_unittest',
            'is_test_apk': 1,
            # Given that this apk tests itself, it needs to bring emma with it
            # when instrumented.
            'conditions': [
              ['emma_coverage != 0', {
                'emma_instrument': 1,
              }],
            ],
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
      ]
    }],
  ]
}
