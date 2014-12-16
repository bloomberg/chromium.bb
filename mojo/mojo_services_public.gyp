# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    'mojo_variables.gypi',
  ],
  'targets': [
    {
      'target_name': 'mojo_services_public',
      'type': 'none',
      'dependencies': [
        'mojo_clipboard_bindings',
        'mojo_content_handler_bindings',
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        'mojo_native_viewport_bindings',
        'mojo_navigation_bindings',
        'mojo_network_bindings',
        'mojo_surface_id_bindings',
        'mojo_surfaces_bindings',
        'mojo_view_manager_bindings',
        'mojo_view_manager_common',
        'mojo_window_manager_bindings',
      ],
    },
    {
      # GN version: //mojo/services/clipboard/public/interfaces
      'target_name': 'mojo_clipboard_bindings',
      'type': 'static_library',
      'sources': [
        'services/clipboard/public/interfaces/clipboard.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/input_events/public/interfaces
      'target_name': 'mojo_input_events_bindings',
      'type': 'static_library',
      'sources': [
        'services/input_events/public/interfaces/input_event_constants.mojom',
        'services/input_events/public/interfaces/input_events.mojom',
        'services/input_events/public/interfaces/input_key_codes.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/geometry/public/interfaces
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'sources': [
        'services/geometry/public/interfaces/geometry.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/gpu/public/interfaces
      'target_name': 'mojo_gpu_bindings',
      'type': 'static_library',
      'sources': [
        'services/gpu/public/interfaces/command_buffer.mojom',
        'services/gpu/public/interfaces/gpu.mojom',
        'services/gpu/public/interfaces/gpu_capabilities.mojom',
        'services/gpu/public/interfaces/viewport_parameter_listener.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/native_viewport/public/interfaces
      'target_name': 'mojo_native_viewport_bindings',
      'type': 'static_library',
      'sources': [
        'services/native_viewport/public/interfaces/native_viewport.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/navigation/public/interfaces
      'target_name': 'mojo_navigation_bindings',
      'type': 'static_library',
      'sources': [
        'services/navigation/public/interfaces/navigation.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_network_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/content_handler/public/interfaces
      'target_name': 'mojo_content_handler_bindings',
      'type': 'static_library',
      'sources': [
        'services/content_handler/public/interfaces/content_handler.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_network_bindings',
        'mojo_public.gyp:mojo_application_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/network/public/interfaces
      'target_name': 'mojo_network_bindings',
      'type': 'static_library',
      'sources': [
        'services/network/public/interfaces/cookie_store.mojom',
        'services/network/public/interfaces/net_address.mojom',
        'services/network/public/interfaces/network_error.mojom',
        'services/network/public/interfaces/network_service.mojom',
        'services/network/public/interfaces/tcp_bound_socket.mojom',
        'services/network/public/interfaces/tcp_connected_socket.mojom',
        'services/network/public/interfaces/tcp_server_socket.mojom',
        'services/network/public/interfaces/udp_socket.mojom',
        'services/network/public/interfaces/url_loader.mojom',
        'services/network/public/interfaces/web_socket.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/view_manager/public/cpp:common
      'target_name': 'mojo_view_manager_common',
      'type': 'static_library',
      'sources': [
        'services/view_manager/public/cpp/types.h',
      ],
    },
    {
      # GN version: //mojo/services/view_manager/public/interfaces/
      'target_name': 'mojo_view_manager_bindings',
      'type': 'static_library',
      'sources': [
        'services/view_manager/public/interfaces/view_manager.mojom',
        'services/view_manager/public/interfaces/view_manager_constants.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_public.gyp:mojo_application_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_public.gyp:mojo_application_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/surfaces/public/interfaces
      'target_name': 'mojo_surfaces_bindings',
      'type': 'static_library',
      'sources': [
        'services/surfaces/public/interfaces/surfaces.mojom',
        'services/surfaces/public/interfaces/surfaces_service.mojom',
        'services/surfaces/public/interfaces/quads.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
        'mojo_surface_id_bindings',
      ],
    },
    {
      # GN version: //mojo/services/surfaces/public/interfaces:surface_id
      'target_name': 'mojo_surface_id_bindings',
      'type': 'static_library',
      'sources': [
        'services/surfaces/public/interfaces/surface_id.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/window_manager/public/interfaces
      'target_name': 'mojo_window_manager_bindings',
      'type': 'static_library',
      'sources': [
        'services/window_manager/public/interfaces/window_manager.mojom',
      ],
      'includes': [ 'mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_input_events_bindings',
        'mojo_public.gyp:mojo_application_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_input_events_bindings',
        'mojo_public.gyp:mojo_application_bindings',
        'mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
  ],
}
