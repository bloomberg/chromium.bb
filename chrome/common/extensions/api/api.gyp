# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'api',
      'type': 'static_library',
      'sources': [
        '<@(idl_schema_files)',
        '<@(json_schema_files)',
      ],
      'includes': [
        '../../../../build/json_schema_bundle_compile.gypi',
        '../../../../build/json_schema_compile.gypi',
      ],
      'variables': {
        'chromium_code': 1,
        'json_schema_files': [
          'events.json',
          'experimental_record.json',
          'permissions.json',
          'storage.json',
          'tabs.json',
          'windows.json',
        ],
        'idl_schema_files': [
          'alarms.idl',
          'experimental_bluetooth.idl',
          'experimental_dns.idl',
          'experimental_idltest.idl',
          'experimental_serial.idl',
          'experimental_socket.idl',
          'experimental_usb.idl',
        ],
        'cc_dir': 'chrome/common/extensions/api',
        'root_namespace': 'extensions::api',
      },
      'conditions': [
        ['OS=="android"', {
          'idl_schema_files!': [
            'experimental_usb.idl',
          ],
        }],
      ],
    },
  ],
}
