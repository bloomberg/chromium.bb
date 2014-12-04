# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copresence_endpoints',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        # GN version: //components/copresence_endpoints
        'copresence_endpoints/copresence_endpoint.cc',
        'copresence_endpoints/copresence_socket.h',
        'copresence_endpoints/public/copresence_endpoint.h',
        'copresence_endpoints/transports/bluetooth/copresence_socket_bluetooth.cc',
        'copresence_endpoints/transports/bluetooth/copresence_socket_bluetooth.h',
      ],
    },
  ],
}
