# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'manager',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/cr_elements/cr_search_field/cr_search_field.js',
          '../../../../ui/webui/resources/js/action_link.js',
          '../../../../ui/webui/resources/js/assert.js',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../ui/webui/resources/js/util.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-a11y-keys-behavior/iron-a11y-keys-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-behaviors/iron-button-state-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-behaviors/iron-control-state-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-list/iron-list-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/iron-resizable-behavior/iron-resizable-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-inky-focus-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-behaviors/paper-ripple-behavior-extracted.js',
          '../../../../third_party/polymer/v1_0/components-chromium/paper-ripple/paper-ripple-extracted.js',
          'action_service.js',
          'constants.js',
          'item.js',
          'toolbar.js',
        ],
        'externs': [
          '<(EXTERNS_DIR)/chrome_send.js',
          'externs.js',
        ],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
