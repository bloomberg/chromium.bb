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
        '../../apps/shell/app_shell.gyp:app_shell_pak',
        '../../ash/ash_resources.gyp:ash_resources',
        '../../content/content_resources.gyp:content_resources',
        '../../webkit/webkit_resources.gyp:webkit_resources',
        '../../webkit/webkit_resources.gyp:webkit_strings',
      ],
      'actions': [{
        'action_name': 'repack_athena_pack',
        'variables': {
          'pak_inputs': [
            '<(PRODUCT_DIR)/app_shell.pak',
            '<(SHARED_INTERMEDIATE_DIR)/ash/resources/ash_resources_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.pak',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.pak',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.pak',
          ],
          'pak_output': '<(PRODUCT_DIR)/athena_resources.pak',
        },
        'includes': [ '../../build/repack_action.gypi' ],
      }],
    },
  ]
}
