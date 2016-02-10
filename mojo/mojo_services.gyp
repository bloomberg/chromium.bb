# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'network_service_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'services/network/public/interfaces/cookie_store.mojom',
          'services/network/public/interfaces/http_connection.mojom',
          'services/network/public/interfaces/http_message.mojom',
          'services/network/public/interfaces/http_server.mojom',
          'services/network/public/interfaces/net_address.mojom',
          'services/network/public/interfaces/network_error.mojom',
          'services/network/public/interfaces/network_service.mojom',
          'services/network/public/interfaces/tcp_bound_socket.mojom',
          'services/network/public/interfaces/tcp_connected_socket.mojom',
          'services/network/public/interfaces/tcp_server_socket.mojom',
          'services/network/public/interfaces/udp_socket.mojom',
          'services/network/public/interfaces/url_loader.mojom',
          'services/network/public/interfaces/url_loader_factory.mojom',
          'services/network/public/interfaces/web_socket.mojom',
          'services/network/public/interfaces/web_socket_factory.mojom',
        ],
        'mojom_include_path': '<(DEPTH)/mojo/services',
      },
      'includes': [
        'mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      # GN version: //mojo/services/network/public/interfaces
      'target_name': 'network_service_bindings_lib',
      'type': 'static_library',
      'dependencies': [
        'network_service_bindings_mojom',
      ],
    },
    {
      # Target used to depend only on the bindings generation action, not on any
      # outputs.
      'target_name': 'network_service_bindings_generation',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'network_service_bindings_mojom',
      ],
    },
    {
      # GN version: //mojo/converters/network
      'target_name': 'network_type_converters',
      'type': 'static_library',
      'dependencies': [
        'network_service_bindings_lib',
      ],
      'sources': [
        'converters/network/network_type_converters.cc',
        'converters/network/network_type_converters.h',
      ],
    },
    {
      'target_name': 'updater_bindings_mojom',
      'type': 'none',
      'variables': {
        'mojom_files': [
          'services/updater/updater.mojom',
        ],
        'mojom_include_path': '<(DEPTH)/mojo/services',
      },
      'includes': [
        'mojom_bindings_generator_explicit.gypi',
      ],
    },
    {
      # GN version: //mojo/services/updater
      'target_name': 'updater_bindings_lib',
      'type': 'static_library',
      'dependencies': [
        'updater_bindings_mojom',
      ],
    },
  ],
}
