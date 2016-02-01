# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'history',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/js/action_link.js',
          '../../../../ui/webui/resources/js/assert.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/alert_overlay.js',
          '../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_grid.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_outline_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_row.js',
          '../../../../ui/webui/resources/js/cr/ui/menu.js',
          '../../../../ui/webui/resources/js/cr/ui/menu_button.js',
          '../../../../ui/webui/resources/js/cr/ui/menu_item.js',
          '../../../../ui/webui/resources/js/cr/ui/overlay.js',
          '../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/compiled_resources.gyp:load_time_data',
          '../../../../ui/webui/resources/js/util.js',
          'history_focus_manager.js',
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
