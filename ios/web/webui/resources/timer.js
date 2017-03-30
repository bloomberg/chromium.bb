// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "timer"
//
// This module provides basic createOneShot, createRepeating support. The
// reason to define this module is to forward calls to setTimeout, setInterval
// exposed by the browser. Mojo JS bindings are currently loaded using gin
// and createOneShot, createRepeating are not always available.
//
// TODO(crbug.com/703380): When the Mojo JS bindings move away from gin,
// this module could be removed.

define("timer", [], function() {
  /**
   * Executes a function once after the set time delay.
   * @param {int} delay Milliseconds to wait before executing the code.
   * @callback callback
   */
  function createOneShot(delay, callback) {
    setTimeout(callback, delay);
  }

  /**
   * Repeatedly executes a function with a fixed time delay between each call.
   * @param {int} delay Milliseconds to wait between executions of the code
   * @callback callback
   */
  function createRepeating(delay, callback) {
    setInterval(callback, delay);
  }

  var exports = {};
  exports.createOneShot = createOneShot;
  exports.createRepeating = createRepeating;
  return exports;
});
