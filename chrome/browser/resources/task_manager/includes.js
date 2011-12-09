// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This script loads additional scripts after initialization of task manager.

var script = [
  'cr/ui/command.js',
  'cr/ui/position_util.js',
  'cr/ui/menu_item.js',
  'cr/ui/menu.js',
  'cr/ui/context_menu_handler.js',
];

/**
 * Loads delayed scripts.
 * This function is called by TaskManager::initalize() in main.js.
 */
function loadDelayedIncludes(taskmanager) {
  // Switch to 'test harness' mode when loading from a file url.
  var isHarness = document.location.protocol == 'file:';

  // In test harness mode we load resources from relative dirs.
  var prefix = isHarness ? './shared/' : 'chrome://resources/';

  // Number of remaining scripts to load.
  var remain = script.length;

  // Waits for initialization of task manager.
  for (var i = 0; i < script.length; ++i) {
    var s = document.createElement('script');
    s.onload = function(e) {
      if (!--remain)
        taskmanager.delayedInitialize();
    };
    s.type = 'text/javascript';
    s.src = prefix + 'js/' + script[i];
    s.defer = 'defer';
    document.body.appendChild(s);
  }
}
