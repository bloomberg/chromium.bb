// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'log-buffer',

  properties: {
    /**
     * List of displayed logs.
     * @type {?Array<{{
     *    text: string,
     *    time: string,
     *    file: string,
     *    line: number,
     *    severity: number,
     * }}>} LogMessage
     */
    logs: {
      type: Array,
      value: [],
      notify: true,
    }
  },

  /**
   * Called when an instance is initialized.
   */
  ready: function() {
    // We assume that only one instance of log-buffer is ever created.
    LogBufferInterface = this;
    chrome.send('getLogMessages');
  },

  // Clears the native LogBuffer.
  clearLogs: function() {
    chrome.send('clearLogBuffer');
  },

  // Handles when a new log message is added.
  onLogMessageAdded: function(log) {
    this.push('logs', log);
  },

  // Handles when the logs are cleared.
  onLogBufferCleared: function() {
    this.logs = [];
  },

  // Handles when the logs are returned in response to the 'getLogMessages'
  // request.
  onGotLogMessages: function(logs) {
    this.logs = logs;
  }
});

// Interface with the native WebUI component for LogBuffer events. The functions
// contained in this object will be invoked by the browser for each operation
// performed on the native LogBuffer.
LogBufferInterface = {
  /**
   * Called when a new log message is added.
   * @type {function(LogMessage)}
   */
  onLogMessageAdded: function(log) {},

  /**
   * Called when the log buffer is cleared.
   * @type {function()}
   */
  onLogBufferCleared: function() {},

  /**
   * Called in response to chrome.send('getLogMessages') with the log messages
   * currently in the buffer.
   * @type {function(Array<LogMessage>)}
   */
  onGotLogMessages: function(messages) {},
};
