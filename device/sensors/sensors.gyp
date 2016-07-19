# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
    'mojom_files': [
      'public/interfaces/light.mojom',
      'public/interfaces/motion.mojom',
      'public/interfaces/orientation.mojom',
    ],
  },
  'targets': [
    {
      'target_name': 'device_sensors_mojo_bindings',
      'type': 'static_library',
      'sources': [ '<@(mojom_files)' ],
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
      'variables': {
        'use_new_wrapper_types': 'false',
      },
    },
    {
      'target_name': 'device_sensors_mojo_bindings_for_blink',
      'type': 'static_library',
      'sources': [ '<@(mojom_files)' ],
      'variables': {
        'for_blink': 'true',
        'use_new_wrapper_types': 'false',
      },
      'includes': [
        '../../mojo/mojom_bindings_generator.gypi',
      ],
    },
  ],
}
