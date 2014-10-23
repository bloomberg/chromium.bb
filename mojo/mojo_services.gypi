# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //mojo/services/clipboard/
      'target_name': 'mojo_clipboard',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_common_lib',
        'services/public/mojo_services_public.gyp:mojo_clipboard_bindings',
        'public/mojo_public.gyp:mojo_cpp_bindings',
        'public/mojo_public.gyp:mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'services/clipboard/clipboard_standalone_impl.cc',
        'services/clipboard/clipboard_standalone_impl.h',
        'services/clipboard/main.cc',
      ],
    },
    {
      # GN version: //mojo/services/clipboard:mojo_clipboard_unittests
      'target_name': 'mojo_clipboard_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'edk/mojo_edk.gyp:mojo_system_impl',
        'mojo_application_manager',
        'mojo_base.gyp:mojo_application_chromium',
        'services/public/mojo_services_public.gyp:mojo_clipboard_bindings',
        'mojo_shell_test_support',
      ],
      'sources': [
        'services/clipboard/clipboard_standalone_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/services/gles2:lib
      'target_name': 'mojo_gles2_lib',
      'type': 'static_library',
      'sources': [
        'services/gles2/command_buffer_type_conversions.cc',
        'services/gles2/command_buffer_type_conversions.h',
        'services/gles2/mojo_buffer_backing.cc',
        'services/gles2/mojo_buffer_backing.h',
      ],
      'dependencies': [
        '../gpu/gpu.gyp:command_buffer_common',
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
        'public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/gles2
      'target_name': 'mojo_gles2_service',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:command_buffer_service',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'mojo_gles2_lib',
      ],
      'export_dependent_settings': [
        'mojo_gles2_lib',
      ],
      'sources': [
        'services/gles2/command_buffer_impl.cc',
        'services/gles2/command_buffer_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/native_viewport
      'target_name': 'mojo_native_viewport_service_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc_surfaces',
        '../skia/skia.gyp:skia',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_geometry_lib',
        'mojo_gles2_service',
        'mojo_input_events_lib',
        'mojo_native_viewport_service_args',
        'mojo_surfaces_lib',
        'services/public/mojo_services_public.gyp:mojo_geometry_bindings',
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
        'services/public/mojo_services_public.gyp:mojo_native_viewport_bindings',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
      ],
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_geometry_bindings',
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
        'services/public/mojo_services_public.gyp:mojo_native_viewport_bindings',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
      ],
      'sources': [
        'services/native_viewport/gpu_impl.cc',
        'services/native_viewport/gpu_impl.h',
        'services/native_viewport/native_viewport_impl.cc',
        'services/native_viewport/native_viewport_impl.h',
        'services/native_viewport/platform_viewport.h',
        'services/native_viewport/platform_viewport_android.cc',
        'services/native_viewport/platform_viewport_headless.cc',
        'services/native_viewport/platform_viewport_headless.h',
        'services/native_viewport/platform_viewport_mac.mm',
        'services/native_viewport/platform_viewport_ozone.cc',
        'services/native_viewport/platform_viewport_stub.cc',
        'services/native_viewport/platform_viewport_win.cc',
        'services/native_viewport/platform_viewport_x11.cc',
        'services/native_viewport/viewport_surface.cc',
        'services/native_viewport/viewport_surface.h',
      ],
      'conditions': [
        ['OS=="win" or OS=="android" or OS=="linux" or OS=="mac"', {
          'sources!': [
            'services/native_viewport/platform_viewport_stub.cc',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'mojo_base.gyp:mojo_jni_headers',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../ui/platform_window/win/win_window.gyp:win_window',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../ui/platform_window/x11/x11_window.gyp:x11_window',
            '../ui/events/platform/x11/x11_events_platform.gyp:x11_events_platform',
          ],
        }],
        ['use_ozone==1', {
          'dependencies': [
            '../ui/ozone/ozone.gyp:ozone',
          ],
        }],
      ],
    },
    {
      'target_name': 'mojo_native_viewport_service',
      'type': 'loadable_module',
      'dependencies': [
        'services/public/mojo_services_public.gyp:mojo_native_viewport_bindings',
        'mojo_native_viewport_service_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_native_viewport_bindings',
      ],
      'sources': [
        'services/native_viewport/main.cc',
      ],
    },
    {
      # GN version: //mojo/services/network:lib
      'target_name': 'mojo_network_service_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_application_chromium',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
      ],
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
      ],
      'sources': [
        'services/network/cookie_store_impl.cc',
        'services/network/cookie_store_impl.h',
        'services/network/net_adapters.cc',
        'services/network/net_adapters.h',
        'services/network/net_address_type_converters.cc',
        'services/network/net_address_type_converters.h',
        'services/network/network_context.cc',
        'services/network/network_context.h',
        'services/network/network_service_impl.cc',
        'services/network/network_service_impl.h',
        'services/network/tcp_bound_socket_impl.cc',
        'services/network/tcp_bound_socket_impl.h',
        'services/network/tcp_connected_socket_impl.cc',
        'services/network/tcp_connected_socket_impl.h',
        'services/network/tcp_server_socket_impl.cc',
        'services/network/tcp_server_socket_impl.h',
        'services/network/udp_socket_impl.cc',
        'services/network/udp_socket_impl.h',
        'services/network/url_loader_impl.cc',
        'services/network/url_loader_impl.h',
        'services/network/web_socket_impl.cc',
        'services/network/web_socket_impl.h',
        'services/public/cpp/network/web_socket_read_queue.cc',
        'services/public/cpp/network/web_socket_read_queue.h',
        'services/public/cpp/network/web_socket_write_queue.cc',
        'services/public/cpp/network/web_socket_write_queue.h',
      ],
    },
    {
      # GN version: //mojo/services/network
      'target_name': 'mojo_network_service',
      'type': 'loadable_module',
      'dependencies': [
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
        'mojo_network_service_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'export_dependent_settings': [
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
      ],
      'sources': [
        'services/network/main.cc',
      ],
    },
    {
      # GN version: //mojo/services/network:unittests
      'target_name': 'mojo_network_service_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        'edk/mojo_edk.gyp:mojo_run_all_unittests',
        'edk/mojo_edk.gyp:mojo_system_impl',
        'mojo_application_manager',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_network_service',
        'mojo_shell_test_support',
        'services/public/mojo_services_public.gyp:mojo_network_bindings',
      ],
      'sources': [
        'services/network/udp_socket_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/services/surfaces
      'target_name': 'mojo_surfaces_service',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_cc_support',
        'mojo_geometry_lib',
        'mojo_surfaces_lib',
        'services/public/mojo_services_public.gyp:mojo_geometry_bindings',
        'services/public/mojo_services_public.gyp:mojo_gpu_bindings',
        'services/public/mojo_services_public.gyp:mojo_surfaces_bindings',
        '<(mojo_system_for_loadable_module)',
      ],
      'includes': [
        'mojo_public_gles2_for_loadable_module.gypi',
      ],
      'sources': [
        'services/surfaces/surfaces_impl.cc',
        'services/surfaces/surfaces_impl.h',
        'services/surfaces/surfaces_service_application.cc',
        'services/surfaces/surfaces_service_application.h',
        'services/surfaces/surfaces_service_impl.cc',
        'services/surfaces/surfaces_service_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/test_service:bindings
      'target_name': 'mojo_test_service_bindings',
      'type': 'static_library',
      'sources': [
         # TODO(tim): Move to services/public/interfaces?
        'services/test_service/test_request_tracker.mojom',
        'services/test_service/test_service.mojom',
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
      # GN version: //mojo/services/test_service
      'target_name': 'mojo_test_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_test_service_bindings',
        'public/mojo_public.gyp:mojo_application_standalone',
        'public/mojo_public.gyp:mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'services/test_service/test_request_tracker_client_impl.cc',
        'services/test_service/test_request_tracker_client_impl.h',
        'services/test_service/test_service_application.cc',
        'services/test_service/test_service_application.h',
        'services/test_service/test_service_impl.cc',
        'services/test_service/test_service_impl.h',
        'services/test_service/test_time_service_impl.cc',
        'services/test_service/test_time_service_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/test_service:request_tracker
      'target_name': 'mojo_test_request_tracker_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_test_service_bindings',
        'public/mojo_public.gyp:mojo_application_standalone',
        'public/mojo_public.gyp:mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'services/test_service/test_request_tracker_client_impl.cc',
        'services/test_service/test_request_tracker_client_impl.h',
        'services/test_service/test_request_tracker_application.cc',
        'services/test_service/test_request_tracker_application.h',
        'services/test_service/test_time_service_impl.cc',
        'services/test_service/test_time_service_impl.h',
        'services/test_service/test_request_tracker_impl.cc',
        'services/test_service/test_request_tracker_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/native_viewport:args
      'target_name': 'mojo_native_viewport_service_args',
      'type': 'static_library',
      'sources': [
        'services/public/cpp/native_viewport/lib/args.cc',
        'services/public/cpp/native_viewport/args.h',
      ],
      'include_dirs': [
        '..'
      ],
    },
  ],
}
