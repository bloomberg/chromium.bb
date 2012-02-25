// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines global variables.
var commands = TaskManagerCommands;
var taskmanager = undefined;  // This will be instantiated in main.js.

/**
 * Invoked when a range of items has changed.
 */
function taskChanged(start, length, tasks) {
  var task = {type: 'change', start: start, length: length, tasks: tasks};

  if (taskmanager) taskmanager.processTaskChange(task);
}

// Enable the taskmanager model before the loading of scripts.
(function () {
  for (var i = 0; i < DEFAULT_COLUMNS.length; i++) {
    if (DEFAULT_COLUMNS[i][3])
      commands.setUpdateColumn(DEFAULT_COLUMNS[i][0], true);
  }
  commands.enableTaskManager();
  commands.setUpdateColumn('canInspect', true);
  commands.setUpdateColumn('canActivate', true);
})();
