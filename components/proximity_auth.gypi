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
        "proximity_auth/base64url.cc",
        "proximity_auth/base64url.h",
        "proximity_auth/bluetooth_connection.cc",
        "proximity_auth/bluetooth_connection.h",
        "proximity_auth/bluetooth_connection_finder.cc",
        "proximity_auth/bluetooth_connection_finder.h",
        "proximity_auth/bluetooth_util.cc",
        "proximity_auth/bluetooth_util_chromeos.cc",
        "proximity_auth/bluetooth_util.h",
        "proximity_auth/client.cc",
        "proximity_auth/client.h",
        "proximity_auth/client_observer.h",
        "proximity_auth/connection.cc",
        "proximity_auth/connection.h",
        "proximity_auth/connection_finder.h",
        "proximity_auth/connection_observer.h",
        "proximity_auth/proximity_auth_system.cc",
        "proximity_auth/proximity_auth_system.h",
        "proximity_auth/remote_device.h",
        "proximity_auth/remote_status_update.cc",
        "proximity_auth/remote_status_update.h",
        "proximity_auth/secure_context.h",
        "proximity_auth/wire_message.cc",
        "proximity_auth/wire_message.h",
      ],
    },
    {
      'target_name': 'cryptauth',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
      ],
      'sources': [
        "proximity_auth/cryptauth/cryptauth_api_call_flow.cc",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.h",
      ],
    },
  ],
}
