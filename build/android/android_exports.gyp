# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'android_exports',
      'type': 'none',
      'inputs': [
        '<(DEPTH)/build/android/android_exports.lst',
      ],
      'outputs': [
        '<(android_linker_script)',
      ],
      'copies': [
        {
          'destination': '<(SHARED_INTERMEDIATE_DIR)',
          'files': [
            '<@(_inputs)',
         ],
        },
      ],
      'conditions': [
        ['component=="static_library"', {
          'link_settings': {
            'ldflags': [
              # Only export symbols that are specified in version script.
              '-Wl,--version-script=<(android_linker_script)',
            ],
            'ldflags!': [
              '-Wl,--exclude-libs=ALL',
            ],
          },
        }],
      ],
    },
  ],
}
