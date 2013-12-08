# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_nfc',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
      ],
      'sources': [
        'nfc_adapter.cc',
        'nfc_adapter.h',
        'nfc_adapter_chromeos.cc',
        'nfc_adapter_chromeos.h',
        'nfc_adapter_factory.cc',
        'nfc_adapter_factory.h',
        'nfc_ndef_record.cc',
        'nfc_ndef_record.h',
        'nfc_peer.cc',
        'nfc_peer.h',
        'nfc_tag.cc',
        'nfc_tag.h',
        'nfc_tag_technology.cc',
        'nfc_tag_technology.h'
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../../build/linux/system.gyp:dbus',
            '../../chromeos/chromeos.gyp:chromeos',
            '../../dbus/dbus.gyp:dbus',
          ]
        }],
      ],
    },
  ],
}
