# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'target_defaults': {
    'variables': {
      'installer_util_target': 0,
    },
    'target_conditions': [
      # This part is shared between the two versions of the target.
      ['installer_util_target==1', {
        'sources': [
          'util/browser_distribution.cc',
          'util/browser_distribution.h',
          'util/chrome_frame_distribution.cc',
          'util/chrome_frame_distribution.h',
          'util/copy_tree_work_item.cc',
          'util/copy_tree_work_item.h',
          'util/create_dir_work_item.cc',
          'util/create_dir_work_item.h',
          'util/create_reg_key_work_item.cc',
          'util/create_reg_key_work_item.h',
          'util/delete_reg_value_work_item.cc',
          'util/delete_reg_value_work_item.h',
          'util/delete_tree_work_item.cc',
          'util/delete_tree_work_item.h',
          'util/google_chrome_distribution.cc',
          'util/google_chrome_distribution.h',
          'util/google_update_constants.cc',
          'util/google_update_constants.h',
          'util/google_update_settings.cc',
          'util/google_update_settings.h',
          'util/install_util.cc',
          'util/install_util.h',
          'util/l10n_string_util.cc',
          'util/l10n_string_util.h',
          'util/move_tree_work_item.cc',
          'util/move_tree_work_item.h',
          'util/self_reg_work_item.cc',
          'util/self_reg_work_item.h',
          'util/set_reg_value_work_item.cc',
          'util/set_reg_value_work_item.h',
          'util/util_constants.cc',
          'util/util_constants.h',
          'util/version.cc',
          'util/version.h',
          'util/work_item.cc',
          'util/work_item.h',
          'util/work_item_list.cc',
          'util/work_item_list.h',
        ],
        'include_dirs': [
          '../..',
        ],
      }],
    ],
  },
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'installer_util',
          'type': '<(library)',
          'msvs_guid': 'EFBB1436-A63F-4CD8-9E99-B89226E782EC',
          'variables': {
            'installer_util_target': 1,
          },
          'dependencies': [
            'installer_util_strings',
            '../chrome.gyp:common_constants',
            '../chrome.gyp:chrome_resources',
            '../chrome.gyp:chrome_strings',
            '../../courgette/courgette.gyp:courgette_lib',
            '../../third_party/bspatch/bspatch.gyp:bspatch',
            '../../third_party/icu/icu.gyp:icui18n',
            '../../third_party/icu/icu.gyp:icuuc',
            '../../third_party/libxml/libxml.gyp:libxml',
            '../../third_party/lzma_sdk/lzma_sdk.gyp:lzma_sdk',
          ],
          'sources': [
            'util/compat_checks.cc',
            'util/compat_checks.h',
            'util/delete_after_reboot_helper.cc',
            'util/delete_after_reboot_helper.h',
            'util/helper.cc',
            'util/helper.h',
            'util/html_dialog.h',
            'util/html_dialog_impl.cc',
            'util/logging_installer.cc',
            'util/logging_installer.h',
            'util/lzma_util.cc',
            'util/lzma_util.h',
            'util/master_preferences.cc',
            'util/master_preferences.h',
            'util/shell_util.cc',
            'util/shell_util.h',
          ],
        },
        {
          'target_name': 'installer_util_nacl_win64',
          'type': '<(library)',
          'msvs_guid': '91016F29-C324-4236-8AA0-032765E71582',
          'variables': {
            'installer_util_target': 1,
          },
          'dependencies': [
            'installer_util_strings',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)',
          ],
          'sources': [
            'util/google_chrome_distribution_dummy.cc',
          ],
          'configurations': {
            'Common_Base': {
              'msvs_target_platform': 'x64',
            },
          },
        },
      ],
    }],
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
