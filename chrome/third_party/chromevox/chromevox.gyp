# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['chromeos==1 and disable_nacl==0 and disable_nacl_untrusted==0', {
      'targets': [
        {
          'target_name': 'chromevox_resources',
          'type': 'none',
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox',
              'files': [
                'chromeVoxChromeBackgroundScript.js',
                'chromeVoxChromeOptionsScript.js',
                'chromeVoxChromePageScript.js',
                'chromeVoxKbExplorerScript.js',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox',
              'files': [
                'chromevox/chromevox-128.png',
                'chromevox/chromevox-16.png',
                'chromevox/chromevox-19.png',
                'chromevox/chromevox-48.png',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/background',
              'files': [
                'chromevox/background/background.html',
                'chromevox/background/chrome_shared2.css',
                'chromevox/background/kbexplorer.html',
                'chromevox/background/options.css',
                'chromevox/background/options.html',
                'chromevox/background/options_widgets.css',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/background/earcons',
              'files': [
                'chromevox/background/earcons/alert_modal.ogg',
                'chromevox/background/earcons/alert_nonmodal.ogg',
                'chromevox/background/earcons/bullet.ogg',
                'chromevox/background/earcons/busy_progress_loop.ogg',
                'chromevox/background/earcons/busy_working_loop.ogg',
                'chromevox/background/earcons/button.ogg',
                'chromevox/background/earcons/chalk.ogg',
                'chromevox/background/earcons/check_off.ogg',
                'chromevox/background/earcons/check_on.ogg',
                'chromevox/background/earcons/collapsed.ogg',
                'chromevox/background/earcons/editable_text.ogg',
                'chromevox/background/earcons/ellipsis.ogg',
                'chromevox/background/earcons/expanded.ogg',
                'chromevox/background/earcons/font_change.ogg',
                'chromevox/background/earcons/invalid_keypress.ogg',
                'chromevox/background/earcons/link.ogg',
                'chromevox/background/earcons/listbox.ogg',
                'chromevox/background/earcons/long_desc.ogg',
                'chromevox/background/earcons/new_mail.ogg',
                'chromevox/background/earcons/object_close.ogg',
                'chromevox/background/earcons/object_delete.ogg',
                'chromevox/background/earcons/object_deselect.ogg',
                'chromevox/background/earcons/object_enter.ogg',
                'chromevox/background/earcons/object_exit.ogg',
                'chromevox/background/earcons/object_open.ogg',
                'chromevox/background/earcons/object_select.ogg',
                'chromevox/background/earcons/paragraph_break.ogg',
                'chromevox/background/earcons/scrolling.ogg',
                'chromevox/background/earcons/scrolling_reverse.ogg',
                'chromevox/background/earcons/search_hit.ogg',
                'chromevox/background/earcons/search_miss.ogg',
                'chromevox/background/earcons/section.ogg',
                'chromevox/background/earcons/selection.ogg',
                'chromevox/background/earcons/selection_reverse.ogg',
                'chromevox/background/earcons/special_content.ogg',
                'chromevox/background/earcons/task_success.ogg',
                'chromevox/background/earcons/wrap_edge.ogg',
                'chromevox/background/earcons/wrap.ogg',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/background/keymaps',
              'files': [
                'chromevox/background/keymaps/classic_keymap.json',
                'chromevox/background/keymaps/experimental.json',
                'chromevox/background/keymaps/flat_keymap.json',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/background/',
              'files': [
                'chromevox/background/mathmaps/',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/chromevox/injected',
              'files': [
                'chromevox/injected/api.js',
                'chromevox/injected/api_util.js',
                'chromevox/injected/mathjax_external_util.js',
                'chromevox/injected/mathjax.js',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/closure',
              'files': [
                'closure/base.js',
                'closure/closure_preinit.js',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/extensions/searchbox',
              'files': [
                'extensions/searchvox/abstract_result.js',
                'extensions/searchvox/constants.js',
                'extensions/searchvox/context_menu.js',
                'extensions/searchvox/results.js',
                'extensions/searchvox/search.js',
                'extensions/searchvox/search_tools.js',
                'extensions/searchvox/util.js',
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/resources/chromeos/chromevox/',
              'files': [
                '_locales/',
              ],
            },
          ],
          'dependencies': [
            '../../../third_party/liblouis/liblouis_untrusted.gyp:liblouis_nacl_wrapper_untrusted',
          ],
        },
      ],
    }],
  ],
}
