# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../mojo_variables.gypi',
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
      # GN version: //mojo/services/public/interfaces/clipboard
      'target_name': 'mojo_clipboard_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/clipboard/clipboard.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/input_events
      'target_name': 'mojo_input_events_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/input_events/input_event_constants.mojom',
        'interfaces/input_events/input_events.mojom',
        'interfaces/input_events/input_key_codes.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/geometry
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/geometry/geometry.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/gpu
      'target_name': 'mojo_gpu_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/gpu/command_buffer.mojom',
        'interfaces/gpu/gpu.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/native_viewport
      'target_name': 'mojo_native_viewport_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/native_viewport/native_viewport.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/navigation
      'target_name': 'mojo_navigation_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/navigation/navigation.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_network_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/content_handler
      'target_name': 'mojo_content_handler_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/content_handler/content_handler.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_network_bindings',
        '../../public/mojo_public.gyp:mojo_application_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/network
      'target_name': 'mojo_network_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/network/cookie_store.mojom',
        'interfaces/network/net_address.mojom',
        'interfaces/network/network_error.mojom',
        'interfaces/network/network_service.mojom',
        'interfaces/network/tcp_bound_socket.mojom',
        'interfaces/network/tcp_connected_socket.mojom',
        'interfaces/network/tcp_server_socket.mojom',
        'interfaces/network/udp_socket.mojom',
        'interfaces/network/url_loader.mojom',
        'interfaces/network/web_socket.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'dependencies': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/cpp/view_manager:common
      'target_name': 'mojo_view_manager_common',
      'type': 'static_library',
      'sources': [
        'cpp/view_manager/types.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/view_manager
      'target_name': 'mojo_view_manager_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/view_manager/view_manager.mojom',
        'interfaces/view_manager/view_manager_constants.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
        '../../public/mojo_public.gyp:mojo_application_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
        'mojo_surface_id_bindings',
        '../../public/mojo_public.gyp:mojo_application_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/surfaces
      'target_name': 'mojo_surfaces_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/surfaces/surfaces.mojom',
        'interfaces/surfaces/surfaces_service.mojom',
        'interfaces/surfaces/quads.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gpu_bindings',
        'mojo_surface_id_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_geometry_bindings',
        'mojo_surface_id_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/surfaces:surface_id
      'target_name': 'mojo_surface_id_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/surfaces/surface_id.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/window_manager
      'target_name': 'mojo_window_manager_bindings',
      'type': 'static_library',
      'sources': [
        'interfaces/window_manager/window_manager.mojom',
      ],
      'includes': [ '../../public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_input_events_bindings',
        '../../public/mojo_public.gyp:mojo_application_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_input_events_bindings',
        '../../public/mojo_public.gyp:mojo_application_bindings',
        '../../public/mojo_public.gyp:mojo_cpp_bindings',
      ],
    },
  ],
}
