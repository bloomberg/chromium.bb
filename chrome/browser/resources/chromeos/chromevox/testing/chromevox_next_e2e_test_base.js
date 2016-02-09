// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['chromevox_e2e_test_base.js']);

/**
 * Base test fixture for ChromeVox Next end to end tests.
 *
 * These tests are identical to ChromeVoxE2ETests except for performing the
 * necessary setup to run ChromeVox Next.
 * @constructor
 * @extends {ChromeVoxE2ETest}
 */
function ChromeVoxNextE2ETest() {
  ChromeVoxE2ETest.call(this);
}

ChromeVoxNextE2ETest.prototype = {
  __proto__: ChromeVoxE2ETest.prototype,

  /**
   * Gets the desktop from the automation API and Launches a new tab with
   * the given document, and runs |callback| when a load complete fires.
   * Arranges to call |testDone()| after |callback| returns.
   * NOTE: Callbacks creatd instide |opt_callback| must be wrapped with
   * |this.newCallback| if passed to asynchonous calls.  Otherwise, the test
   * will be finished prematurely.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function(chrome.automation.AutomationNode)} callback
   *     Called once the document is ready.
   */
  runWithLoadedTree: function(doc, callback) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop(function(r) {
      this.runWithTab(doc, function(newTabUrl) {
        var listener = function(evt) {
          if (!evt.target.docUrl || evt.target.docUrl != newTabUrl)
            return;

          r.removeEventListener('loadComplete', listener, true);
          global.backgroundObj.onGotCommand('nextElement');
          callback && callback(evt.target);
          callback = null;
        };
        r.addEventListener('loadComplete', listener, true);
      }.bind(this));
    }.bind(this));
  },

  listenOnce: function(node, eventType, callback, capture) {
    var innerCallback = this.newCallback(function() {
      node.removeEventListener(eventType, innerCallback, capture);
      callback.apply(this, arguments);
    });
    node.addEventListener(eventType, innerCallback, capture);
  }
};
