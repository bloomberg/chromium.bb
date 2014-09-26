# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'proximity_auth',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth',
        '../net/net.gyp:net',
      ],
      'sources': [
        "proximity_auth/bluetooth_connection.cc",
        "proximity_auth/bluetooth_connection.h",
        "proximity_auth/bluetooth_util.cc",
        "proximity_auth/bluetooth_util_chromeos.cc",
        "proximity_auth/bluetooth_util.h",
        "proximity_auth/connection.cc",
        "proximity_auth/connection.h",
        "proximity_auth/connection_observer.h",
        "proximity_auth/proximity_auth_system.cc",
        "proximity_auth/proximity_auth_system.h",
        "proximity_auth/remote_device.h",
        "proximity_auth/wire_message.cc",
        "proximity_auth/wire_message.h",
      ],
    },
  ],
}
