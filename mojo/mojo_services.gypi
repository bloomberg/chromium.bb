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
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_utility',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_clipboard_bindings',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'services/clipboard/clipboard_standalone_impl.cc',
        'services/clipboard/clipboard_standalone_impl.h',
        'services/clipboard/main.cc',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/clipboard
      'target_name': 'mojo_clipboard_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/clipboard/clipboard.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
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
        'mojo_application_manager',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_run_all_unittests',
        'mojo_base.gyp:mojo_system_impl',
        'mojo_clipboard_bindings',
        'mojo_shell_test_support',
      ],
      'sources': [
        'services/clipboard/clipboard_standalone_unittest.cc',
      ],
    },
    {
      # GN version: //mojo/services/html_viewer
      'target_name': 'mojo_html_viewer',
      'type': 'loadable_module',
      'dependencies': [
        '../cc/blink/cc_blink.gyp:cc_blink',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../media/blink/media_blink.gyp:media_blink',
        '../media/media.gyp:media',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../ui/native_theme/native_theme.gyp:native_theme',
        '../url/url.gyp:url_lib',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_common_lib',
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_utility',
        'mojo_clipboard_bindings',
        'mojo_cc_support',
        'mojo_content_handler_bindings',
        'mojo_gpu_bindings',
        'mojo_navigation_bindings',
        'mojo_network_bindings',
        'mojo_surfaces_bindings',
        'mojo_view_manager_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'include_dirs': [
        'third_party/WebKit'
      ],
      'sources': [
        'services/html_viewer/blink_basic_type_converters.cc',
        'services/html_viewer/blink_basic_type_converters.h',
        'services/html_viewer/blink_input_events_type_converters.cc',
        'services/html_viewer/blink_input_events_type_converters.h',
        'services/html_viewer/blink_platform_impl.cc',
        'services/html_viewer/blink_platform_impl.h',
        'services/html_viewer/blink_url_request_type_converters.cc',
        'services/html_viewer/blink_url_request_type_converters.h',
        'services/html_viewer/html_viewer.cc',
        'services/html_viewer/html_document_view.cc',
        'services/html_viewer/html_document_view.h',
        'services/html_viewer/webclipboard_impl.cc',
        'services/html_viewer/webclipboard_impl.h',
        'services/html_viewer/webcookiejar_impl.cc',
        'services/html_viewer/webcookiejar_impl.h',
        'services/html_viewer/webmediaplayer_factory.cc',
        'services/html_viewer/webmediaplayer_factory.h',
        'services/html_viewer/webmimeregistry_impl.cc',
        'services/html_viewer/webmimeregistry_impl.h',
        'services/html_viewer/websockethandle_impl.cc',
        'services/html_viewer/websockethandle_impl.h',
        'services/html_viewer/webstoragenamespace_impl.cc',
        'services/html_viewer/webstoragenamespace_impl.h',
        'services/html_viewer/webthemeengine_impl.cc',
        'services/html_viewer/webthemeengine_impl.h',
        'services/html_viewer/webthread_impl.cc',
        'services/html_viewer/webthread_impl.h',
        'services/html_viewer/weburlloader_impl.cc',
        'services/html_viewer/weburlloader_impl.h',
        'services/html_viewer/weblayertreeview_impl.cc',
        'services/html_viewer/weblayertreeview_impl.h',
        'services/public/cpp/network/web_socket_read_queue.cc',
        'services/public/cpp/network/web_socket_read_queue.h',
        'services/public/cpp/network/web_socket_write_queue.cc',
        'services/public/cpp/network/web_socket_write_queue.h',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/input_events
      'target_name': 'mojo_input_events_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_INPUT_EVENTS_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_input_events_bindings',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'services/public/cpp/input_events/lib/input_events_type_converters.cc',
        'services/public/cpp/input_events/lib/mojo_extended_key_event_data.cc',
        'services/public/cpp/input_events/lib/mojo_extended_key_event_data.h',
        'services/public/cpp/input_events/input_events_type_converters.h',
        'services/public/cpp/input_events/mojo_input_events_export.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/input_events
      'target_name': 'mojo_input_events_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/input_events/input_event_constants.mojom',
        'services/public/interfaces/input_events/input_events.mojom',
        'services/public/interfaces/input_events/input_key_codes.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_geometry_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_geometry_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/geometry
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/geometry/geometry.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/geometry
      'target_name': 'mojo_geometry_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_GEOMETRY_IMPLEMENTATION',
      ],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_geometry_bindings',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'services/public/cpp/geometry/lib/geometry_type_converters.cc',
        'services/public/cpp/geometry/geometry_type_converters.h',
        'services/public/cpp/geometry/mojo_geometry_export.h',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/surfaces
      'target_name': 'mojo_surfaces_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_SURFACES_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../skia/skia.gyp:skia',
        '../gpu/gpu.gyp:gpu',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_geometry_lib',
        'mojo_surfaces_bindings',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        'mojo_geometry_lib',
        'mojo_surfaces_bindings',
      ],
      'sources': [
        'services/public/cpp/surfaces/lib/surfaces_type_converters.cc',
        'services/public/cpp/surfaces/lib/surfaces_utils.cc',
        'services/public/cpp/surfaces/surfaces_type_converters.h',
        'services/public/cpp/surfaces/surfaces_utils.h',
        'services/public/cpp/surfaces/mojo_surfaces_export.h',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/surfaces/tests
      'target_name': 'mojo_surfaces_lib_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../gpu/gpu.gyp:gpu',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_base.gyp:mojo_run_all_unittests',
        'mojo_geometry_lib',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
      ],
      'sources': [
        'services/public/cpp/surfaces/tests/surface_unittest.cc',
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
        'mojo_base.gyp:mojo_gles2_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_gles2_bindings',
      ],
      'sources': [
        'services/gles2/command_buffer_impl.cc',
        'services/gles2/command_buffer_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/gpu
      'target_name': 'mojo_gpu_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/gpu/gpu.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_geometry_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_geometry_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/native_viewport
      'target_name': 'mojo_native_viewport_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/native_viewport/native_viewport.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
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
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_gles2_service',
        'mojo_gpu_bindings',
        'mojo_input_events_lib',
        'mojo_native_viewport_bindings',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_native_viewport_bindings',
        'mojo_surfaces_bindings',
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
        'mojo_native_viewport_bindings',
        'mojo_native_viewport_service_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'export_dependent_settings': [
        'mojo_native_viewport_bindings',
      ],
      'sources': [
        'services/native_viewport/main.cc',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/navigation
      'target_name': 'mojo_navigation_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/navigation/navigation.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_network_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/content_handler
      'target_name': 'mojo_content_handler_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/content_handler/content_handler.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_base.gyp:mojo_application_bindings',
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_network_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/network
      'target_name': 'mojo_network_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/network/cookie_store.mojom',
        'services/public/interfaces/network/network_error.mojom',
        'services/public/interfaces/network/network_service.mojom',
        'services/public/interfaces/network/url_loader.mojom',
        'services/public/interfaces/network/web_socket.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
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
        'mojo_network_bindings',
      ],
      'export_dependent_settings': [
        'mojo_network_bindings',
      ],
      'sources': [
        'services/network/cookie_store_impl.cc',
        'services/network/cookie_store_impl.h',
        'services/network/network_context.cc',
        'services/network/network_context.h',
        'services/network/network_service_impl.cc',
        'services/network/network_service_impl.h',
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
        'mojo_network_bindings',
        'mojo_network_service_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'export_dependent_settings': [
        'mojo_network_bindings',
      ],
      'sources': [
        'services/network/main.cc',
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
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_gpu_bindings',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
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
      # GN version: //mojo/services/public/cpp/view_manager:common
      'target_name': 'mojo_view_manager_common',
      'type': 'static_library',
      'sources': [
        'services/public/cpp/view_manager/types.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/view_manager
      'target_name': 'mojo_view_manager_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/view_manager/view_manager.mojom',
        'services/public/interfaces/view_manager/view_manager_constants.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_application_bindings',
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_application_bindings',
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/view_manager
      'target_name': 'mojo_view_manager_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc_surfaces',
        '../gpu/gpu.gyp:gpu',
        '../skia/skia.gyp:skia',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../third_party/khronos/khronos.gyp:khronos_headers',
        'mojo_base.gyp:mojo_application_chromium',
        'mojo_base.gyp:mojo_application_bindings',
        'mojo_core_window_manager_bindings',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
        'mojo_view_manager_bindings',
        'mojo_view_manager_common',
        'mojo_gpu_bindings',
      ],
      'includes': [
        'mojo_public_gles2_for_loadable_module.gypi',
      ],
      'sources': [
        'services/public/cpp/view_manager/lib/bitmap_uploader.cc',
        'services/public/cpp/view_manager/lib/bitmap_uploader.h',
        'services/public/cpp/view_manager/lib/view.cc',
        'services/public/cpp/view_manager/lib/view_manager_client_factory.cc',
        'services/public/cpp/view_manager/lib/view_manager_client_impl.cc',
        'services/public/cpp/view_manager/lib/view_manager_client_impl.h',
        'services/public/cpp/view_manager/lib/view_manager_context.cc',
        'services/public/cpp/view_manager/lib/view_observer.cc',
        'services/public/cpp/view_manager/lib/view_private.cc',
        'services/public/cpp/view_manager/lib/view_private.h',
        'services/public/cpp/view_manager/view.h',
        'services/public/cpp/view_manager/view_manager.h',
        'services/public/cpp/view_manager/view_manager_client_factory.h',
        'services/public/cpp/view_manager/view_manager_context.h',
        'services/public/cpp/view_manager/view_manager_delegate.h',
        'services/public/cpp/view_manager/view_observer.h',
        'services/public/cpp/view_manager/window_manager_delegate.h',
      ],
      'export_dependent_settings': [
        'mojo_gpu_bindings',
        'mojo_surfaces_bindings',
        'mojo_view_manager_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/view_manager/tests:mojo_view_manager_lib_unittests
      'target_name': 'mojo_view_manager_lib_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'mojo_base.gyp:mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_shell_test_support',
        'mojo_view_manager_bindings',
        'mojo_view_manager_lib',
      ],
      'sources': [
        'services/public/cpp/view_manager/tests/view_unittest.cc',
        'services/public/cpp/view_manager/tests/view_manager_unittest.cc',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            'mojo_view_manager_run_unittests'
          ],
        }, {  # use_aura==0
          'dependencies': [
            'mojo_base.gyp:mojo_run_all_unittests',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/surfaces
      'target_name': 'mojo_surfaces_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/surfaces/surfaces.mojom',
        'services/public/interfaces/surfaces/surfaces_service.mojom',
        'services/public/interfaces/surfaces/quads.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_geometry_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
        'mojo_base.gyp:mojo_gles2_bindings',
        'mojo_geometry_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/surfaces:surface_id
      'target_name': 'mojo_surface_id_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/surfaces/surface_id.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
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
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/test_service
      'target_name': 'mojo_test_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_base.gyp:mojo_utility',
        'mojo_base.gyp:mojo_application_standalone',
        'mojo_test_service_bindings',
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
        'mojo_base.gyp:mojo_utility',
        'mojo_base.gyp:mojo_application_standalone',
        'mojo_test_service_bindings',
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
      # GN version: //mojo/services/public/interfaces/window_manager
      'target_name': 'mojo_core_window_manager_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/window_manager/window_manager.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_base.gyp:mojo_cpp_bindings',
      ],
    },
  ],
  'conditions': [
    ['use_aura==1', {
      'targets': [
        {
          # GN version: //mojo/services/view_manager
          'target_name': 'mojo_view_manager',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../cc/cc.gyp:cc_surfaces',
            '../skia/skia.gyp:skia',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/events/events.gyp:events',
            '../ui/events/events.gyp:events_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'mojo_base.gyp:mojo_common_lib',
            'mojo_base.gyp:mojo_application_chromium',
            'mojo_base.gyp:mojo_common_lib',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_gpu_bindings',
            'mojo_input_events_bindings',
            'mojo_input_events_lib',
            'mojo_native_viewport_bindings',
            'mojo_surfaces_bindings',
            'mojo_surfaces_lib',
            'mojo_view_manager_bindings',
            'mojo_view_manager_common',
            'mojo_gpu_bindings',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'services/view_manager/access_policy.h',
            'services/view_manager/access_policy_delegate.h',
            'services/view_manager/connection_manager.cc',
            'services/view_manager/connection_manager.h',
            'services/view_manager/default_access_policy.cc',
            'services/view_manager/default_access_policy.h',
            'services/view_manager/display_manager.cc',
            'services/view_manager/display_manager.h',
            'services/view_manager/ids.h',
            'services/view_manager/main.cc',
            'services/view_manager/server_view.cc',
            'services/view_manager/server_view.h',
            'services/view_manager/server_view_delegate.h',
            'services/view_manager/view_manager_export.h',
            'services/view_manager/view_manager_init_service_context.cc',
            'services/view_manager/view_manager_init_service_context.h',
            'services/view_manager/view_manager_init_service_impl.cc',
            'services/view_manager/view_manager_init_service_impl.h',
            'services/view_manager/view_manager_service_impl.cc',
            'services/view_manager/view_manager_service_impl.h',
            'services/view_manager/window_manager_access_policy.cc',
            'services/view_manager/window_manager_access_policy.h',
          ],
          'includes': [
            'mojo_public_gles2_for_loadable_module.gypi',
          ],
          'defines': [
            'MOJO_VIEW_MANAGER_IMPLEMENTATION',
          ],
        },
        {
          # GN version: //mojo/services/public/cpp/view_manager/lib:run_unittests
          'target_name': 'mojo_view_manager_run_unittests',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
          ],
          'sources': [
            'services/public/cpp/view_manager/lib/view_manager_test_suite.cc',
            'services/public/cpp/view_manager/lib/view_manager_test_suite.h',
            'services/public/cpp/view_manager/lib/view_manager_unittests.cc',
          ],
          'conditions': [
            ['use_x11==1', {
              'dependencies': [
                '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
              ],
            }],
          ],
        },
        {
          # GN version: //mojo/services/view_manager:mojo_view_manager_unittests
          'target_name': 'mojo_view_manager_unittests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../ui/aura/aura.gyp:aura',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            'mojo_application_manager',
            'mojo_base.gyp:mojo_system_impl',
            'mojo_base.gyp:mojo_application_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_input_events_bindings',
            'mojo_input_events_lib',
            'mojo_shell_test_support',
            'mojo_view_manager_bindings',
            'mojo_view_manager_common',
            'mojo_view_manager_run_unittests',
            # Included only to force deps for bots.
            'mojo_native_viewport_service',
            'mojo_surfaces_service',
            'mojo_view_manager',
          ],
          'sources': [
            'services/view_manager/test_change_tracker.cc',
            'services/view_manager/test_change_tracker.h',
            'services/view_manager/view_manager_unittest.cc',
          ],
          'conditions': [
             ['OS=="win"', {
               'dependencies': [
                 '../ui/gfx/gfx.gyp:gfx',
               ],
             }],
           ],
        },
        {
          'target_name': 'package_mojo_view_manager',
          'variables': {
            'app_name': 'mojo_view_manager',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
        {
          # GN version: //mojo/services/window_manager:lib
          'target_name': 'mojo_core_window_manager_lib',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/wm/wm.gyp:wm',
            'mojo_base.gyp:mojo_common_lib',
            'mojo_base.gyp:mojo_application_chromium',
            'mojo_aura_support',
            'mojo_core_window_manager_bindings',
            'mojo_input_events_lib',
            'mojo_view_manager_lib',
          ],
          'sources': [
            'services/window_manager/window_manager_app.cc',
            'services/window_manager/window_manager_app.h',
            'services/window_manager/window_manager_service_impl.cc',
            'services/window_manager/window_manager_service_impl.h',
          ],
        },
        {
          # GN version: //mojo/services/window_manager
          'target_name': 'mojo_core_window_manager',
          'type': 'loadable_module',
          'dependencies': [
            'mojo_core_window_manager_lib',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'services/window_manager/main.cc',
          ],
        },
        {
          # GN version: //mojo/services/window_manager:mojo_core_window_manager_unittests
          'target_name': 'mojo_core_window_manager_unittests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            'mojo_application_manager',
            'mojo_base.gyp:mojo_system_impl',
            'mojo_base.gyp:mojo_environment_chromium',
            'mojo_core_window_manager_bindings',
            'mojo_shell_test_support',
            'mojo_view_manager_bindings',
            'mojo_view_manager_lib',
          ],
          'sources': [
            'services/window_manager/window_manager_api_unittest.cc',
            'services/window_manager/window_manager_unittests.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../third_party/mesa/mesa.gyp:osmesa',
                'mojo_native_viewport_service_lib',
              ],
            }],
            ['use_x11==1', {
              'dependencies': [
                '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
