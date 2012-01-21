// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of utility methods for UberPage and its contained
 *     pages.
 */

cr.define('uber', function() {
  /**
   * Invokes a method on the parent window (UberPage). This is a convenience
   * method for API calls into the uber page.
   * @param {String} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @private
   */
  function invokeMethodOnParent(method, opt_params) {
    if (window.location == window.parent.location)
      return;

    invokeMethodOnWindow(window.parent, method, opt_params, 'chrome://chrome');
  }

  /**
   * Invokes a method on the target window.
   * @param {String} method The name of the method to invoke.
   * @param {Object=} opt_params Optional property bag of parameters to pass to
   *     the invoked method.
   * @param {String=} opt_url The origin of the target window.
   * @private
   */
  function invokeMethodOnWindow(targetWindow, method, opt_params, opt_url) {
    var data = {method: method, params: opt_params};
    targetWindow.postMessage(data, opt_url ? opt_url : '*');
  }

  return {
    invokeMethodOnParent: invokeMethodOnParent,
    invokeMethodOnWindow: invokeMethodOnWindow,
  };
});
