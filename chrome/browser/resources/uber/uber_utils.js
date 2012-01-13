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
   * @param {Object} params Optional property bag of parameters to pass to the
   *     invoked method.
   * @private
   */
  function invokeMethodOnParent(method, params) {
    if (!window.parent)
      return;

    var data = {method: method, params: params};
    window.parent.postMessage(data, 'chrome://chrome');
  };

  return {
    invokeMethodOnParent: invokeMethodOnParent
  }
});
