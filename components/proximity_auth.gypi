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
        "proximity_auth/bluetooth_util.h",
        "proximity_auth/bluetooth_util_chromeos.cc",
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
        "proximity_auth/switches.cc",
        "proximity_auth/switches.h",
        "proximity_auth/wire_message.cc",
        "proximity_auth/wire_message.h",
      ],
    },
    {
      # GN version: //components/cryptauth/proto
      'target_name': 'cryptauth_proto',
      'type': 'static_library',
      'sources': [
        'proximity_auth/cryptauth/proto/cryptauth_api.proto',
      ],
      'variables': {
        'proto_in_dir': 'proximity_auth/cryptauth/proto',
        'proto_out_dir': 'components/proximity_auth/cryptauth/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
    {
      'target_name': 'cryptauth',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        'cryptauth_proto',
        '../base/base.gyp:base',
        '../google_apis/google_apis.gyp:google_apis',
        '../net/net.gyp:net',
      ],
      'sources': [
        "proximity_auth/cryptauth/cryptauth_access_token_fetcher.h",
        "proximity_auth/cryptauth/cryptauth_account_token_fetcher.cc",
        "proximity_auth/cryptauth/cryptauth_account_token_fetcher.h",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.cc",
        "proximity_auth/cryptauth/cryptauth_api_call_flow.h",
        "proximity_auth/cryptauth/cryptauth_client.cc",
        "proximity_auth/cryptauth/cryptauth_client.h",
        "proximity_auth/cryptauth/cryptauth_enrollment_utils.cc",
        "proximity_auth/cryptauth/cryptauth_enrollment_utils.h",
      ],
      'export_dependent_settings': [
        'cryptauth_proto',
      ],
    },
  ],
}
