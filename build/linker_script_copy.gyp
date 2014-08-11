# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'linker_script_copy',
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
    },
  ],
}
