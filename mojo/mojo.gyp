# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'conditions': [
      ['mojo_shell_debug_url != ""', {
        'defines': [
          'MOJO_SHELL_DEBUG=1',
          'MOJO_SHELL_DEBUG_URL="<(mojo_shell_debug_url)"',
         ],
      }],
    ],
  },
  'includes': [
    'mojo_converters.gypi',
    'mojo_services.gypi',
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      # GN version: //mojo
      'target_name': 'mojo',
      'type': 'none',
      'dependencies': [
        'edk/mojo_edk.gyp:mojo_edk',
        'mojo_application_manager',
        'mojo_application_manager_unittests',
        'mojo_base.gyp:mojo_base',
        'mojo_clipboard',
        'mojo_clipboard_unittests',
        'mojo_geometry_lib',
        'mojo_input_events_lib',
        'mojo_js_unittests',
        'mojo_native_viewport_service',
        'mojo_network_service',
        'mojo_shell',
        'mojo_shell_lib',
        'mojo_shell_tests',
        'mojo_surfaces_lib',
        'mojo_surfaces_lib_unittests',
        'mojo_surfaces_service',
        'mojo_test_app',
        'mojo_test_request_tracker_app',
        'services/public/mojo_services_public.gyp:mojo_services_public',
        'public/mojo_public.gyp:mojo_public',
      ],
      'conditions': [
        ['OS == "linux"', {
          'dependencies': [
            'mojo_external_application_tests',
          ],
        }],
      ]
    },
    {
      # GN version: //mojo/shell:external_service_bindings
      'target_name': 'mojo_external_service_bindings',
      'type': 'static_library',
      'sources': [
        'shell/external_service.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/spy
      'target_name': 'mojo_spy',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:http_server',
        '../url/url.gyp:url_lib',
        'mojo_application_manager',
      ],
      'variables': {
        'mojom_base_output_dir': 'mojo',
      },
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'sources': [
        'spy/public/spy.mojom',
        'spy/common.h',
        'spy/spy.cc',
        'spy/spy.h',
        'spy/spy_server_impl.h',
        'spy/spy_server_impl.cc',
        'spy/websocket_server.cc',
        'spy/websocket_server.h',
      ],
    },
    {
      # GN version: //mojo/shell:lib
      'target_name': 'mojo_shell_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_static',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'edk/mojo_edk.gyp:mojo_system_impl',
        'mojo_application_manager',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_external_service_bindings',
        'mojo_gles2_impl',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
        'mojo_spy',
        'public/mojo_public.gyp:mojo_application_bindings',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'sources': [
        'shell/app_child_process.cc',
        'shell/app_child_process.h',
        'shell/app_child_process.mojom',
        'shell/app_child_process_host.cc',
        'shell/app_child_process_host.h',
        'shell/child_process.cc',
        'shell/child_process.h',
        'shell/child_process_host.cc',
        'shell/child_process_host.h',
        'shell/context.cc',
        'shell/context.h',
        'shell/dynamic_application_loader.cc',
        'shell/dynamic_application_loader.h',
        'shell/dynamic_service_runner.h',
        'shell/external_application_listener.h',
        'shell/external_application_listener_posix.cc',
        'shell/external_application_listener_win.cc',
        'shell/external_application_registrar.mojom',
        'shell/filename_util.cc',
        'shell/filename_util.h',
        'shell/in_process_dynamic_service_runner.cc',
        'shell/in_process_dynamic_service_runner.h',
        'shell/incoming_connection_listener_posix.cc',
        'shell/incoming_connection_listener_posix.h',
        'shell/init.cc',
        'shell/init.h',
        'shell/mojo_url_resolver.cc',
        'shell/mojo_url_resolver.h',
        'shell/out_of_process_dynamic_service_runner.cc',
        'shell/out_of_process_dynamic_service_runner.h',
        'shell/switches.cc',
        'shell/switches.h',
        'shell/task_runners.cc',
        'shell/task_runners.h',
        'shell/test_child_process.cc',
        'shell/test_child_process.h',
        'shell/ui_application_loader_android.cc',
        'shell/ui_application_loader_android.h',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'dependencies': [
            '../ui/gl/gl.gyp:gl',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            'shell/external_application_registrar_connection.cc',
            'shell/external_application_registrar_connection.h',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'mojo_network_service_lib',
            'mojo_native_viewport_service_lib',
          ],
          'sources': [
            'shell/network_application_loader.cc',
            'shell/network_application_loader.h',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/shell:test_support
      'target_name': 'mojo_shell_test_support',
      'type': 'static_library',
      'dependencies': [
        'edk/mojo_edk.gyp:mojo_system_impl',
        'mojo_shell_lib',
      ],
      'sources': [
        'shell/shell_test_helper.cc',
        'shell/shell_test_helper.h',
      ],
    },
    {
      # GN version: //mojo/shell
      'target_name': 'mojo_shell',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_shell_lib',
      ],
      'sources': [
        'shell/desktop/mojo_main.cc',
      ],
      'conditions': [
        ['component=="shared_library"', {
          'dependencies': [
            '../ui/gfx/gfx.gyp:gfx',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/shell:mojo_shell_tests
      'target_name': 'mojo_shell_tests',
      'type': '<(gtest_target_type)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../net/net.gyp:net_test_support',
        '../url/url.gyp:url_lib',
        'edk/mojo_edk.gyp:mojo_system_impl',
        'mojo_application_manager',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_shell_lib',
        'mojo_test_app',
        'mojo_test_request_tracker_app',
        'mojo_test_service_bindings',
      ],
      'sources': [
        'shell/child_process_host_unittest.cc',
        'shell/dynamic_application_loader_unittest.cc',
        'shell/in_process_dynamic_service_runner_unittest.cc',
        'shell/mojo_url_resolver_unittest.cc',
        'shell/shell_test_base.cc',
        'shell/shell_test_base.h',
        'shell/shell_test_base_unittest.cc',
        'shell/shell_test_main.cc',
      ],
      'conditions': [
        ['OS == "android"', {
          'dependencies': [
            '../testing/android/native_test.gyp:native_test_native_code',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/application_manager
      'target_name': 'mojo_application_manager',
      'type': '<(component)',
      'defines': [
        'MOJO_APPLICATION_MANAGER_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../url/url.gyp:url_lib',
        'services/public/mojo_services_public.gyp:mojo_content_handler_bindings',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'public/mojo_public.gyp:mojo_application_bindings',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'application_manager/application_loader.cc',
        'application_manager/application_loader.h',
        'application_manager/application_manager.cc',
        'application_manager/application_manager.h',
        'application_manager/application_manager_export.h',
        'application_manager/background_shell_application_loader.cc',
        'application_manager/background_shell_application_loader.h',
      ],
      'export_dependent_settings': [
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        'public/mojo_public.gyp:mojo_application_bindings',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
      ],
    },
    {
      # GN version: //mojo/application_manager:mojo_application_manager_unittests
      'target_name': 'mojo_application_manager_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_application_manager',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_environment_chromium',
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'sources': [
        'application_manager/application_manager_unittest.cc',
        'application_manager/background_shell_application_loader_unittest.cc',
        'application_manager/test.mojom',
      ],
    },
    {
      # GN version: //mojo/cc
      'target_name': 'mojo_cc_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../skia/skia.gyp:skia',
        '../gpu/gpu.gyp:gles2_implementation',
        'mojo_geometry_lib',
        'mojo_surfaces_lib',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
      ],
      'includes': [
        'mojo_public_gles2_for_loadable_module.gypi',
      ],
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
      ],
      'sources': [
        'cc/context_provider_mojo.cc',
        'cc/context_provider_mojo.h',
        'cc/direct_output_surface.cc',
        'cc/direct_output_surface.h',
        'cc/output_surface_mojo.cc',
        'cc/output_surface_mojo.h',
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
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_gles2_lib',
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
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
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
      ],
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
      # GN version: //mojo/bindings/js/tests:mojo_js_unittests
      'target_name': 'mojo_js_unittests',
      'type': 'executable',
      'dependencies': [
        '../gin/gin.gyp:gin_test',
        'edk/mojo_edk.gyp:mojo_common_test_support',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'mojo_base.gyp:mojo_js_bindings_lib',
        'public/mojo_public.gyp:mojo_environment_standalone',
        'public/mojo_public.gyp:mojo_public_test_interfaces',
        'public/mojo_public.gyp:mojo_utility',
      ],
      'sources': [
        'bindings/js/tests/run_js_tests.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="android"', {
      'targets': [
        {
          'target_name': 'mojo_native_viewport_java',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
          ],
          'variables': {
            'java_in_dir': '<(DEPTH)/mojo/services/native_viewport/android',
          },
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'libmojo_shell',
          'type': 'shared_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            'mojo_base.gyp:mojo_common_lib',
            'mojo_base.gyp:mojo_environment_chromium',
            'mojo_base.gyp:mojo_jni_headers',
            'mojo_shell_lib',
            'public/mojo_public.gyp:mojo_application_bindings',
          ],
          'sources': [
            'shell/android/library_loader.cc',
            'shell/android/mojo_main.cc',
            'shell/android/mojo_main.h',
          ],
        },
        {
          'target_name': 'mojo_shell_apk',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_java',
            '../net/net.gyp:net_java',
            'mojo_native_viewport_java',
            'libmojo_shell',
          ],
          'variables': {
            'apk_name': 'MojoShell',
            'java_in_dir': '<(DEPTH)/mojo/shell/android/apk',
            'resource_dir': '<(DEPTH)/mojo/shell/android/apk/res',
            'native_lib_target': 'libmojo_shell',
          },
          'includes': [ '../build/java_apk.gypi' ],
        }
      ],
    }],
    ['OS=="linux"', {
      'targets': [
        {
          # GN version: //mojo/shell:mojo_external_application_tests
          'target_name': 'mojo_external_application_tests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            '../net/net.gyp:net_test_support',
            '../url/url.gyp:url_lib',
            'edk/mojo_edk.gyp:mojo_system_impl',
            'mojo_application_manager',
            'mojo_base.gyp:mojo_common_lib',
            'mojo_base.gyp:mojo_environment_chromium',
            'mojo_shell_lib',
          ],
          'sources': [
            'shell/incoming_connection_listener_unittest.cc',
            'shell/external_application_listener_unittest.cc',
            'shell/external_application_test_main.cc',
          ],
        },
      ],
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
          ],
          'sources': [
            'mojo_js_unittests.isolate',
          ],
        },
      ],
    }],
  ],
}
