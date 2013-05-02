// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

TaskManagerCommands = {
  /**
   * Sends commands to kill selected processes.
   */
  killSelectedProcesses: function(uniqueIds) {
    chrome.send('killProcesses', uniqueIds);
  },

  /**
   * Sends command to initiate resource inspection.
   */
  inspect: function(uniqueId) {
    chrome.send('inspect', [uniqueId]);
  },

  /**
   * Sends command to open about memory tab.
   */
  openAboutMemory: function() {
    chrome.send('openAboutMemory');
  },

  /**
   * Sends command to disable taskmanager model.
   */
  disableTaskManager: function() {
    chrome.send('disableTaskManager');
  },

  /**
   * Sends command to enable taskmanager model.
   */
  enableTaskManager: function() {
    chrome.send('enableTaskManager');
  },

  /**
   * Sends command to activate a page.
   */
  activatePage: function(uniqueId) {
    chrome.send('activatePage', [uniqueId]);
  },

  /**
   * Sends command to enable or disable the given columns to update the data.
   */
  setUpdateColumn: function(columnId, isEnabled) {
    chrome.send('setUpdateColumn', [columnId, isEnabled]);

    // The 'title' column contains the icon.
    if (columnId == 'title')
      chrome.send('setUpdateColumn', ['icon', isEnabled]);
  }
};
