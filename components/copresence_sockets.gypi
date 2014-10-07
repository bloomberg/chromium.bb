# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'copresence_sockets',
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
        # GN version: //components/copresence_sockets
        'copresence_sockets/copresence_peer.cc',
        'copresence_sockets/transports/bluetooth/copresence_socket_bluetooth.cc',
        'copresence_sockets/transports/bluetooth/copresence_socket_bluetooth.h',
        'copresence_sockets/public/copresence_peer.h',
        'copresence_sockets/public/copresence_socket.h',
      ],
    },
  ],
}
