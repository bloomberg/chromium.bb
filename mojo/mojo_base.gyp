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
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            'mojo_public.gyp:mojo_bindings_java',
            'mojo_public.gyp:mojo_public_java',
          ],
        }],
      ]
    },
    {
      'target_name': 'mojo_none',
      'type': 'none',
    },
    {
      # GN version: //mojo/message_pump
      'target_name': 'mojo_message_pump_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_MESSAGE_PUMP_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'sources': [
        'message_pump/handle_watcher.cc',
        'message_pump/handle_watcher.h',
        'message_pump/message_pump_mojo.cc',
        'message_pump/message_pump_mojo.h',
        'message_pump/message_pump_mojo_handler.h',
        'message_pump/time_helper.cc',
        'message_pump/time_helper.h',
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
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'sources': [
        'common/common_type_converters.cc',
        'common/common_type_converters.h',
        'common/data_pipe_file_utils.cc',
        'common/data_pipe_utils.cc',
        'common/data_pipe_utils.h',
      ],
    },
    {
      # GN version: //mojo/common:url_type_converters
      'target_name': 'mojo_url_type_converters',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../url/url.gyp:url_lib',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
      ],
      'sources': [
        'common/url_type_converters.cc',
        'common/url_type_converters.h',
      ],
    },
    {
      # GN version: //mojo/converters/geometry
      'target_name': 'mojo_geometry_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_GEOMETRY_IMPLEMENTATION',
      ],
      'dependencies': [
        '../ui/mojo/geometry/mojo_bindings.gyp:mojo_geometry_bindings',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_environment_chromium',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'converters/geometry/geometry_type_converters.cc',
        'converters/geometry/geometry_type_converters.h',
        'converters/geometry/mojo_geometry_export.h',
      ],
    },
    {
      # GN version: //mojo/common:mojo_common_unittests
      'target_name': 'mojo_common_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../base/base.gyp:base_message_loop_tests',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'mojo_common_lib',
        'mojo_edk.gyp:mojo_system_impl',
        'mojo_edk.gyp:mojo_common_test_support',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_environment_chromium',
        'mojo_message_pump_lib',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_public.gyp:mojo_public_test_utils',
        'mojo_url_type_converters',
      ],
      'sources': [
        'common/common_type_converters_unittest.cc',
        'message_pump/handle_watcher_unittest.cc',
        'message_pump/message_pump_mojo_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/environment:chromium
      'target_name': 'mojo_environment_chromium',
      'type': 'static_library',
      'dependencies': [
        'mojo_environment_chromium_impl',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'sources': [
        # TODO(vtl): This is kind of ugly. (See TODO in logging.h.)
        "../mojo/public/cpp/environment/async_waiter.h",
        "../mojo/public/cpp/environment/lib/async_waiter.cc",
        "../mojo/public/cpp/environment/lib/logging.cc",
        "../mojo/public/cpp/environment/lib/scoped_task_tracking.cc",
        "../mojo/public/cpp/environment/lib/scoped_task_tracking.cc",
        "../mojo/public/cpp/environment/logging.h",
        "../mojo/public/cpp/environment/task_tracker.h",
        'environment/environment.cc',
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
        'mojo_message_pump_lib',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'environment/default_async_waiter_impl.cc',
        'environment/default_async_waiter_impl.h',
        'environment/default_logger_impl.cc',
        'environment/default_logger_impl.h',
        'environment/default_run_loop_impl.cc',
        'environment/default_run_loop_impl.h',
        'environment/default_task_tracker_impl.cc',
        'environment/default_task_tracker_impl.h',
      ],
      'include_dirs': [
        '..',
      ],
    },
    {
      'target_name': 'mojo_application_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'shell/public/interfaces/application_manager.mojom',
          'shell/public/interfaces/content_handler.mojom',
          'shell/public/interfaces/interface_provider.mojom',
          'shell/public/interfaces/shell.mojom',
          'shell/public/interfaces/shell_client.mojom',
        ],
      },
      'dependencies': [
        'mojo_services.gyp:network_service_bindings_generation',
      ],
      'export_dependent_settings': [
        'mojo_services.gyp:network_service_bindings_generation',
      ],
      'includes': [ 'mojom_bindings_generator_explicit.gypi' ],
    },
    {
      # GN version: //mojo/shell/public/cpp
      'target_name': 'mojo_application_base',
      'type': 'static_library',
      'sources': [
        'shell/public/cpp/application_runner.h',
        'shell/public/cpp/connect.h',
        'shell/public/cpp/connection.h',
        'shell/public/cpp/initialize_base_and_icu.cc',
        'shell/public/cpp/initialize_base_and_icu.h',
        'shell/public/cpp/interface_binder.h',
        'shell/public/cpp/interface_factory.h',
        'shell/public/cpp/interface_factory_impl.h',
        'shell/public/cpp/interface_registry.h',
        'shell/public/cpp/lib/application_runner.cc',
        'shell/public/cpp/lib/connection_impl.cc',
        'shell/public/cpp/lib/connection_impl.h',
        'shell/public/cpp/lib/interface_factory_binder.h',
        'shell/public/cpp/lib/interface_registry.cc',
        'shell/public/cpp/lib/shell_client.cc',
        'shell/public/cpp/lib/shell_connection.cc',
        'shell/public/cpp/shell.h',
        'shell/public/cpp/shell_client.h',
        'shell/public/cpp/shell_connection.h',
      ],
      'dependencies': [
        '../base/base.gyp:base_i18n',
        'mojo_application_bindings',
        'mojo_message_pump_lib',
        'mojo_services.gyp:network_type_converters',
      ],
    },
    {
      # GN version: //mojo/public/interfaces/application:application
      'target_name': 'mojo_application_bindings',
      'type': 'static_library',
      'dependencies': [
        'mojo_application_bindings_mojom',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_services.gyp:network_service_bindings_lib',
      ],
      'export_dependent_settings': [
        'mojo_services.gyp:network_service_bindings_lib',
      ],
    },
    {
      # GN version: //mojo/test:test_support
      'target_name': 'mojo_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
      ],
      'sources': [
        'test/test_utils.h',
        'test/test_utils_posix.cc',
        'test/test_utils_win.cc',
      ],
    },
    {
      # GN version: //mojo/shell/public/cpp/tests
      'target_name': 'mojo_public_application_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        'mojo_application_base',
        'mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_public.gyp:mojo_utility',
        'mojo_public.gyp:mojo_environment_standalone',
      ],
      'sources': [
        'shell/public/cpp/tests/interface_registry_unittest.cc',
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
          ],
          'variables': {
            'jni_gen_package': 'mojo',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
        {
          'target_name': 'libmojo_system_java',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            'mojo_common_lib',
            'mojo_edk.gyp:mojo_system_impl',
            'mojo_environment_chromium',
            'mojo_jni_headers',
          ],
          'sources': [
            'android/system/core_impl.cc',
            'android/system/core_impl.h',
          ],
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
            'libmojo_system_java',
            'mojo_public.gyp:mojo_public_java',
          ],
          'variables': {
            'java_in_dir': '<(DEPTH)/mojo/android/system',
          },
          'includes': [ '../build/java.gypi' ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'mojo_common_unittests_run',
          'type': 'none',
          'dependencies': [
            'mojo_common_unittests',
          ],
          'includes': [
            '../build/isolate.gypi',
          ],
          'sources': [
            'mojo_common_unittests.isolate',
          ],
        },
      ],
    }],
  ]
}
