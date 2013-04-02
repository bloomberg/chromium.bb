# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file is meant to be included into a target to provide a rule
# to build uiautomator dexed tests jar.
#
# To use this, create a gyp target with the following form:
# {
#   'target_name': 'test_suite_name',
#   'type': 'none',
#   'includes': ['path/to/this/gypi/file'],
# }
#

{
  'dependencies': [
    '<(DEPTH)/tools/android/android_tools.gyp:android_tools',
  ],
  'variables': {
    'output_dex_path': '<(PRODUCT_DIR)/lib.java/<(_target_name).dex.jar',
  },
  'actions': [
    {
      'action_name': 'dex_<(_target_name)',
      'message': 'Dexing <(_target_name) jar',
      'inputs': [
        '<(DEPTH)/build/android/pylib/build_utils.py',
        '<(DEPTH)/build/android/dex.py',
        '>@(library_dexed_jars_paths)',
      ],
      'outputs': [
        '<(output_dex_path)',
      ],
      'action': [
        'python', '<(DEPTH)/build/android/dex.py',
        '--dex-path=<(output_dex_path)',
        '--android-sdk-root=<(android_sdk_root)',

        # TODO(newt): remove this once http://crbug.com/177552 is fixed in ninja.
        '--ignore=>!(echo \'>(_inputs)\' | md5sum)',

        '>@(library_dexed_jars_paths)',
      ],
    },
  ],
}
