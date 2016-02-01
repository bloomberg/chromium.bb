# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Closure compiler definition for MD History. Not tied into the main closure
# build while we get oriented with the system.
# To compile:
#   GYP_GENERATORS=ninja tools/gyp/gyp --depth . -Goutput_dir=out_closure \
#     chrome/browser/resources/md_history/compiled_resources.gyp
#   ninja -C out_closure/Default
{
  'targets': [
    {
      'target_name': 'history',

      'variables': {
        'depends': [
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/util.js',
          '../../../../ui/webui/resources/js/cr/ui/position_util.js',
          'history_card_manager.js',
          'history_card.js',
          'history_item.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          '../history/externs.js',
        ],
      },

      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    },
  ],
}
