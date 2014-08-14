# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Essential components (and their tests) that are needed to build
# Chrome should be here.  Other components that are useful only in
# Mojo land like mojo_shell should be in mojo.gyp.
{
  'includes': [
    'mojo_public.gypi',
    'mojo_public_tests.gypi',
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_base',
      'type': 'none',
      'dependencies': [
        'mojo_common_lib',
        'mojo_common_unittests',
        'mojo_cpp_bindings',
        'mojo_js_bindings',
        'mojo_js_unittests',
        'mojo_message_generator',
        'mojo_public_application_unittests',
        'mojo_public_test_utils',
        'mojo_public_bindings_unittests',
        'mojo_public_environment_unittests',
        'mojo_public_system_perftests',
        'mojo_public_system_unittests',
        'mojo_public_utility_unittests',
        'mojo_system',
        'mojo_system_impl',
        'mojo_system_unittests',
        'mojo_utility',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'mojo_bindings_java',
            'mojo_public_java',
            'mojo_system_java',
            'libmojo_system_java',
            'mojo_test_apk',
          ],
        }],
      ]
    },
    {
      'target_name': 'mojo_none',
      'type': 'none',
    },
    {
      'target_name': 'mojo_run_all_unittests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system_impl',
        'mojo_test_support',
        'mojo_test_support_impl',
      ],
      'sources': [
        'common/test/run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'mojo_run_all_perftests',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:test_support_base',
        'mojo_system_impl',
        'mojo_test_support',
        'mojo_test_support_impl',
      ],
      'sources': [
        'common/test/run_all_perftests.cc',
      ],
    },
    {
      # GN version: //mojo/system
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
        'embedder/scoped_platform_handle.h',
        'system/channel.cc',
        'system/channel.h',
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
        'system/raw_shared_buffer.cc',
        'system/raw_shared_buffer.h',
        'system/raw_shared_buffer_posix.cc',
        'system/raw_shared_buffer_win.cc',
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
      'target_name': 'mojo_system_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_test_support',
        'mojo_system_impl',
      ],
      'sources': [
        'embedder/embedder_unittest.cc',
        'embedder/platform_channel_pair_posix_unittest.cc',
        'system/channel_unittest.cc',
        'system/core_unittest.cc',
        'system/core_test_base.cc',
        'system/core_test_base.h',
        'system/data_pipe_unittest.cc',
        'system/dispatcher_unittest.cc',
        'system/local_data_pipe_unittest.cc',
        'system/memory_unittest.cc',
        'system/message_pipe_dispatcher_unittest.cc',
        'system/message_pipe_unittest.cc',
        'system/multiprocess_message_pipe_unittest.cc',
        'system/options_validation_unittest.cc',
        'system/platform_handle_dispatcher_unittest.cc',
        'system/raw_channel_unittest.cc',
        'system/raw_shared_buffer_unittest.cc',
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
    },
    {
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
      'target_name': 'mojo_common_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'mojo_system_impl',
      ],
      'sources': [
        'common/test/multiprocess_test_helper.cc',
        'common/test/multiprocess_test_helper.h',
        'common/test/test_utils.h',
        'common/test/test_utils_posix.cc',
        'common/test/test_utils_win.cc',
      ],
    },
    {
      'target_name': 'mojo_common_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_message_loop_tests',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'mojo_cpp_bindings',
        'mojo_environment_chromium',
        'mojo_common_lib',
        'mojo_common_test_support',
        'mojo_public_test_utils',
        'mojo_run_all_unittests',
      ],
      'sources': [
        'common/common_type_converters_unittest.cc',
        'common/handle_watcher_unittest.cc',
        'common/message_pump_mojo_unittest.cc',
        'common/test/multiprocess_test_helper_unittest.cc',
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
      # GN version: //mojo/services/gles2:interfaces (for files generated from
      # the mojom file)
      # GN version: //mojo/services/gles2:bindings
      'target_name': 'mojo_gles2_bindings',
      'type': 'static_library',
      'sources': [
        'services/gles2/command_buffer.mojom',
        'services/gles2/command_buffer_type_conversions.cc',
        'services/gles2/command_buffer_type_conversions.h',
        'services/gles2/mojo_buffer_backing.cc',
        'services/gles2/mojo_buffer_backing.h',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
        '../gpu/gpu.gyp:command_buffer_common',
      ],
    },
    {
      # GN version: //mojo/gles2
      'target_name': 'mojo_gles2_impl',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../gpu/gpu.gyp:command_buffer_client',
        '../gpu/gpu.gyp:command_buffer_common',
        '../gpu/gpu.gyp:gles2_cmd_helper',
        '../gpu/gpu.gyp:gles2_implementation',
        'mojo_environment_chromium',
        'mojo_gles2_bindings',
        '<(mojo_system_for_component)',
      ],
      'defines': [
        'GLES2_USE_MOJO',
        'GL_GLEXT_PROTOTYPES',
        'MOJO_GLES2_IMPLEMENTATION',
        'MOJO_GLES2_IMPL_IMPLEMENTATION',
        'MOJO_USE_GLES2_IMPL'
      ],
      'direct_dependent_settings': {
        'defines': [
          'GLES2_USE_MOJO',
        ],
      },
      'sources': [
        'gles2/command_buffer_client_impl.cc',
        'gles2/command_buffer_client_impl.h',
        'gles2/gles2_impl_export.h',
        'gles2/gles2_impl.cc',
        'gles2/gles2_context.cc',
        'gles2/gles2_context.h',
      ],
      'all_dependent_settings': {
        # Ensures that dependent projects import the core functions on Windows.
        'defines': ['MOJO_USE_GLES2_IMPL'],
      }
    },
    {
     'target_name': 'mojo_application_chromium',
     'type': 'static_library',
     'sources': [
       'public/cpp/application/lib/application_impl_chromium.cc',
      ],
      'dependencies': [
        'mojo_application_base',
       ],
      'export_dependent_settings': [
        'mojo_application_base',
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
        'bindings/js/handle.cc',
        'bindings/js/handle.h',
        'bindings/js/support.cc',
        'bindings/js/support.h',
        'bindings/js/waiting_callback.cc',
        'bindings/js/waiting_callback.h',
      ],
    },
    {
      'target_name': 'mojo_message_generator',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_common_lib',
        'mojo_cpp_bindings',
        'mojo_environment_chromium',
        'mojo_system_impl',
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
            'mojo_public_java',
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
          ],
          'defines': [
            'UNIT_TEST'  # As exported from testing/gtest.gyp:gtest.
          ],
          'sources': [
            'android/javatests/mojo_test_case.cc',
            'android/javatests/mojo_test_case.h',
            'android/javatests/init_library.cc',
          ],
        },
        {
          'target_name': 'mojo_test_apk',
          'type': 'none',
          'dependencies': [
            'mojo_bindings_java',
            'mojo_public_test_interfaces',
            'mojo_system_java',
            '../base/base.gyp:base_java_test_support',
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
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_js_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_js_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
            'mojo_js_unittests.isolate',
          ],
          'sources': [
            'mojo_js_unittests.isolate',
          ],
        },
      ],
    }],
  ]
}
