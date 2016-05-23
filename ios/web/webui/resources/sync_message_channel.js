// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "ios/mojo/public/js/sync_message_channel"
//
// This module provides the synchronous communication channel between
// JavaScript and native code, which serves as mojo backend.

define("ios/mojo/public/js/sync_message_channel", [], function() {
  var exports = {};

  /**
   * Synchronously sends a message to mojo backend.
   * @param {!Object} message to send.
   * @return {Object} Response from mojo backend.
   */
  exports.sendMessage = function(message) {
    var response = window.prompt(__gCrWeb.common.JSONStringify(message));
    return response ? JSON.parse(response) : undefined;
  }

  return exports;
});
