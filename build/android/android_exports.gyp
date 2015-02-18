# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This target is only used when android_webview_build==1 - it implements a
# whitelist for exported symbols to minimise the binary size and prevent us
# accidentally exposing things we don't mean to expose.

{
  'variables': {
    'android_linker_script%': '<(SHARED_INTERMEDIATE_DIR)/android_webview_export_whitelist.lst',
  },
  'targets': [
    {
      'target_name': 'android_exports',
      'type': 'none',
      'inputs': [
        '<(DEPTH)/build/android/android_webview_export_whitelist.lst',
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
          },
        }],
      ],
    },
  ],
}
