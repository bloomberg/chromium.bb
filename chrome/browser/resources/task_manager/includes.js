// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script includes additional resources via document.write(). Hence, it
// must be a separate script file loaded before other scripts which would
// reference the resources.

var css = [
  'chrome_shared2.css',
  'list.css',
  'table.css',
  'menu.css',
  'widgets.css',
];

var script = [
  'load_time_data.js',
  'i18n_template_no_process.js',

  'event_tracker.js',
  'util.js',
  'cr.js',
  'cr/ui.js',
  'cr/event_target.js',
  'cr/ui/array_data_model.js',
  'cr/ui/list_item.js',
  'cr/ui/list_selection_model.js',
  'cr/ui/list_single_selection_model.js',
  'cr/ui/list_selection_controller.js',
  'cr/ui/list.js',

  'cr/ui/splitter.js',
  'cr/ui/table/table_splitter.js',
  'cr/ui/touch_handler.js',

  'cr/ui/table/table_column.js',
  'cr/ui/table/table_column_model.js',
  'cr/ui/table/table_header.js',
  'cr/ui/table/table_list.js',
  'cr/ui/table.js',

  'cr/ui/grid.js',
];

var scriptDelayed = [
  'cr/ui/command.js',
  'cr/ui/position_util.js',
  'cr/ui/menu_item.js',
  'cr/ui/menu.js',
  'cr/ui/context_menu_handler.js',
];

var loadDelayedIncludes;

(function() {
  // Switch to 'test harness' mode when loading from a file url.
  var isHarness = document.location.protocol == 'file:';

  // In test harness mode we load resources from relative dirs.
  var prefix = isHarness ? './shared/' : 'chrome://resources/';

  for (var i = 0; i < css.length; i++) {
    document.write('<link href="' + prefix + 'css/' + css[i] +
                   '" rel="stylesheet"></link>');
  }

  for (var i = 0; i < script.length; i++) {
    document.write('<script src="' + prefix + 'js/' + script[i] +
                   '"><\/script>');
  }

  /**
   * Loads delayed scripts.
   * This function is called by TaskManager::initalize() in main.js.
   */
  loadDelayedIncludes = function(taskmanager) {
    // Number of remaining scripts to load.
    var remain = scriptDelayed.length;

    // Waits for initialization of task manager.
    for (var i = 0; i < scriptDelayed.length; i++) {
      var s = document.createElement('script');
      s.onload = function(e) {
        if (!--remain)
          taskmanager.delayedInitialize();
      };
      s.src = prefix + 'js/' + scriptDelayed[i];
      document.head.appendChild(s);
    }
  };
})();
