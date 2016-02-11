# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Closure compiler definition for MD History.
{
  'targets': [
    {
      'target_name': 'history',

      'variables': {
        'depends': [
          '../../../../third_party/polymer/v1_0/components-chromium/iron-a11y-keys-behavior/iron-a11y-keys-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-list/iron-list-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-resizable-behavior/iron-resizable-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-scroll-target-behavior/iron-scroll-target-behavior-extracted.js',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../ui/webui/resources/js/util.js',
          'history.js',
          'history_card.js',
          'history_card_manager.js',
          'history_item.js',
          'history_toolbar.js',
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
