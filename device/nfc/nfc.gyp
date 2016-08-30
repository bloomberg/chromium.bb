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
