# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'android_support_v13_target%':
    '../../third_party/android_tools/android_tools.gyp:android_support_v13_javalib',
  },
  'targets': [
    {
      # GN version: //device/nfc
      'target_name': 'device_nfc',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../url/url.gyp:url_lib',
      ],
      'sources': [
        # Note: file list duplicated in GN build.
        'nfc_adapter.cc',
        'nfc_adapter.h',
        'nfc_adapter_chromeos.cc',
        'nfc_adapter_chromeos.h',
        'nfc_adapter_factory.cc',
        'nfc_adapter_factory.h',
        'nfc_ndef_record.cc',
        'nfc_ndef_record.h',
        'nfc_ndef_record_utils_chromeos.cc',
        'nfc_ndef_record_utils_chromeos.h',
        'nfc_peer.cc',
        'nfc_peer.h',
        'nfc_peer_chromeos.cc',
        'nfc_peer_chromeos.h',
        'nfc_tag.cc',
        'nfc_tag.h',
        'nfc_tag_chromeos.cc',
        'nfc_tag_chromeos.h',
        'nfc_tag_technology.cc',
        'nfc_tag_technology.h',
        'nfc_tag_technology_chromeos.cc',
        'nfc_tag_technology_chromeos.h'
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
    {
      'target_name': 'device_nfc_mojo_bindings',
      'type': 'static_library',
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'nfc.mojom',
      ],
      'variables': {
        'use_new_wrapper_types': 'false',
      },
    },
    {
      'target_name': 'device_nfc_mojo_bindings_for_blink',
      'type': 'static_library',
      'variables': {
        'for_blink': 'true',
        'use_new_wrapper_types': 'false',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'sources': [
        'nfc.mojom',
      ],
    },
  ],
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'device_nfc_java',
          'type': 'none',
          'dependencies': [
            '<(android_support_v13_target)',
            '../../base/base.gyp:base',
            '../../mojo/mojo_public.gyp:mojo_bindings_java',
            'device_nfc_mojo_bindings',
          ],
          'variables': {
            'java_in_dir': '../../device/nfc/android/java',
          },
          'includes': [ '../../build/java.gypi' ],
        },
      ],
    }, {  # OS != "android"
    }],
  ],
}
