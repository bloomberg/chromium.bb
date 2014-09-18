# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'options_bundle',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/css/tree.css.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/array_data_model.js',
          '../../../../ui/webui/resources/js/cr/ui/autocomplete_list.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble_button.js',
          '../../../../ui/webui/resources/js/cr/ui/command.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_outline_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/grid.js',
          '../../../../ui/webui/resources/js/cr/ui/list.js',
          '../../../../ui/webui/resources/js/cr/ui/list_item.js',
          '../../../../ui/webui/resources/js/cr/ui/list_selection_controller.js',
          '../../../../ui/webui/resources/js/cr/ui/list_selection_model.js',
          '../../../../ui/webui/resources/js/cr/ui/list_single_selection_model.js',
          '../../../../ui/webui/resources/js/cr/ui/menu.js',
          '../../../../ui/webui/resources/js/cr/ui/menu_item.js',
          '../../../../ui/webui/resources/js/cr/ui/overlay.js',
          '../../../../ui/webui/resources/js/cr/ui/position_util.js',
          '../../../../ui/webui/resources/js/cr/ui/page_manager/page.js',
          '../../../../ui/webui/resources/js/cr/ui/page_manager/page_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/repeating_button.js',
          '../../../../ui/webui/resources/js/cr/ui/touch_handler.js',
          '../../../../ui/webui/resources/js/cr/ui/tree.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../ui/webui/resources/js/parse_html_subset.js',
          '../../../../ui/webui/resources/js/util.js',
          '../../../../chrome/browser/resources/chromeos/keyboard/keyboard_utils.js',
        ],
        'externs': ['<(CLOSURE_DIR)/externs/chrome_send_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
