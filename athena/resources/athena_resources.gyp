# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'athena_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/athena/resources',
      },
      'actions': [
        {
          'action_name': 'athena_resources',
          'variables': {
            'grit_grd_file': 'athena_resources.grd',
          },
          'includes': [ '../../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../../build/grit_target.gypi' ],
    },
    {
      'target_name': 'athena_pak',
      'type': 'none',
      'dependencies': [
        '../../ash/ash_resources.gyp:ash_resources',
        '../../components/components_resources.gyp:components_resources',
        '../../components/components_strings.gyp:components_strings',
        '../../content/app/resources/content_resources.gyp:content_resources',
        '../../content/app/strings/content_strings.gyp:content_strings',
        '../../extensions/extensions.gyp:extensions_shell_and_test_pak',
        '../../ui/chromeos/ui_chromeos.gyp:ui_chromeos_resources',
        '../../ui/chromeos/ui_chromeos.gyp:ui_chromeos_strings',
        'athena_resources',
      ],
      'actions': [{
        'action_name': 'repack_athena_pack',
        'variables': {
          'pak_inputs': [
            '<(PRODUCT_DIR)/extensions_shell_and_test.pak',
            '<(SHARED_INTERMEDIATE_DIR)/ash/resources/ash_resources_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/athena/resources/athena_resources_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/athena/strings/athena_strings_en-US.pak',
            '<(SHARED_INTERMEDIATE_DIR)/components/components_resources_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/components/components_resources.pak',
            '<(SHARED_INTERMEDIATE_DIR)/components/strings/components_strings_en-US.pak',
            '<(SHARED_INTERMEDIATE_DIR)/content/app/resources/content_resources_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/resources/ui_chromeos_resources_100_percent.pak',
            '<(SHARED_INTERMEDIATE_DIR)/ui/chromeos/strings/ui_chromeos_strings_en-US.pak',
          ],
          'pak_output': '<(PRODUCT_DIR)/athena_resources.pak',
        },
        'includes': [ '../../build/repack_action.gypi' ],
      }],
    },
  ]
}
