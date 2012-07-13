// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function WindowStateManager() {
  this.savedWindow = [];
}

/**
 * Minimize all opening windows and save their states.
 */
WindowStateManager.prototype.saveStates = function() {
  var focusedWindowId;
  chrome.windows.getLastFocused(function(focusedWindow) {
    focusedWindowId = focusedWindow.id;
  });
  var self = this;
  chrome.windows.getAll(null, function(windows) {
    for (var i in windows) {
      if (windows[i].state != 'minimized' &&
          windows[i].id != focusedWindowId) {
        self.savedWindow.push(windows[i]);
        chrome.windows.update(windows[i].id, {'state': 'minimized'},
                              function() {});
      }
    }
  });
};

/**
 * Restore the states of all windows.
 */
WindowStateManager.prototype.restoreStates = function() {
  for (var i in this.savedWindow) {
     var state = this.savedWindow[i].state;
     chrome.windows.update(this.savedWindow[i].id, {'state': state},
                           function() {});
  }
};

/**
 * Remove the saved state of the current focused window.
 * @param {integer} windowId The ID of the window.
 */
WindowStateManager.prototype.removeSavedState = function(windowId) {
  for (var i in this.savedWindow) {
    if (windowId == this.savedWindow[i].id)
      this.savedWindow.splice(i, 1);
  }
};

var windowStateManager = new WindowStateManager();

// If a window gets focused before wallpaper manager close. The saved state
// is no longer correct.
chrome.windows.onFocusChanged.addListener(
    windowStateManager.removeSavedState.bind(windowStateManager));
