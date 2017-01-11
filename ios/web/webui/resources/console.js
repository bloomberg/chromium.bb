// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "console"
//
// This module provides basic logging support. The reason to define this module
// to forward calls to the |console| object exposed by the browser, instead of
// using that object directly: mojo JS bindings are currently loaded using gin
// and the |console| object is not always available. When the Mojo JS bindings
// move away from gin, this module could be removed.

define("console", [], function() {
  /**
   * Logs a message to the console.
   * @param {string} message to log.
   */
  function log(message) {
    console.log(message);
  }

  var exports = {};
  exports.log = log;
  return exports;
});
