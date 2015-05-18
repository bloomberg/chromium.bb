# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [{
    # GN version: //mojo/services/network/public/interfaces
    'target_name': 'network_service_bindings_mojom',
    'type': 'static_library',
    'variables': {
      'mojom_files': [
        'services/network/public/interfaces/cookie_store.mojom',
        'services/network/public/interfaces/http_connection.mojom',
        'services/network/public/interfaces/http_server.mojom',
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
      'mojom_include_path': '<(DEPTH)/mojo/services',
    },
    'includes': [
      '../third_party/mojo/mojom_bindings_generator_explicit.gypi',
    ],
  }, {
    # GN version: //third_party/mojo_services/src/content_handler/public/interfaces
    'target_name': 'content_handler_bindings_mojom',
    'type': 'static_library',
    'variables': {
      'mojom_files': [
        '../third_party/mojo_services/src/content_handler/public/interfaces/content_handler.mojom',
      ],
      'mojom_include_path': '<(DEPTH)/mojo/services',
    },
    'includes': [
      '../third_party/mojo/mojom_bindings_generator_explicit.gypi',
    ],
    'dependencies': [
      '<(DEPTH)/mojo/mojo_services.gyp:network_service_bindings_mojom',
      '<(DEPTH)/third_party/mojo/mojo_public.gyp:mojo_application_bindings_mojom',
    ],
  }],
}
