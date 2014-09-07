# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'help',
      'variables': {
        'depends': [
          '../../../../ui/webui/resources/js/assert.js',
          '../../../../ui/webui/resources/js/cr.js',
          '../../../../ui/webui/resources/js/cr/event_target.js',
          '../../../../ui/webui/resources/js/cr/ui.js',
          '../../../../ui/webui/resources/js/cr/ui/bubble.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/focus_outline_manager.js',
          '../../../../ui/webui/resources/js/cr/ui/overlay.js',
          '../../../../ui/webui/resources/js/cr/ui/page_manager/page.js',
          '../../../../ui/webui/resources/js/cr/ui/page_manager/page_manager.js',
          '../../../../ui/webui/resources/js/event_tracker.js',
          '../../../../ui/webui/resources/js/load_time_data.js',
          '../../../../ui/webui/resources/js/util.js',
          '../../../../chrome/browser/resources/help/channel_change_page.js',
          '../../../../chrome/browser/resources/help/help_page.js',
        ],
        'externs': ['<(CLOSURE_DIR)/externs/chrome_send_externs.js'],
      },
      'includes': ['../../../../third_party/closure_compiler/compile_js.gypi'],
    }
  ],
}
