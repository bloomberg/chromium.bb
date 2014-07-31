# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_pak',
      'type': 'none',
      'dependencies': [
        '../../ash/ash_resources.gyp:ash_resources',
        '../../extensions/extensions.gyp:extensions_shell_and_test_pak',
      ],
      'actions': [{
        'action_name': 'repack_athena_pack',
        'variables': {
          'pak_inputs': [
            '<(PRODUCT_DIR)/extensions_shell_and_test.pak',
            '<(SHARED_INTERMEDIATE_DIR)/ash/resources/ash_resources_100_percent.pak',
          ],
          'pak_output': '<(PRODUCT_DIR)/athena_resources.pak',
        },
        'includes': [ '../../build/repack_action.gypi' ],
      }],
    },
  ]
}
