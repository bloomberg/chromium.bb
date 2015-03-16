// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['chrome/browser/resources/chromeos/chromevox/testing/' +
    'chromevox_e2e_test_base.js']);

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
   * Launches a new tab with the given document, and runs callback when a load
   * complete fires.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function()} opt_callback Called once the document is ready.
   */
  runWithLoadedTree: function(doc, callback) {
    callback = this.newCallback(callback);
    chrome.automation.getDesktop(function(r) {
      var listener = function(evt) {
        if (!evt.target.attributes.url ||
            evt.target.attributes.url.indexOf('test') == -1)
          return;

        r.removeEventListener(listener);
        callback && callback(evt.target);
        callback = null;
      };
      r.addEventListener('loadComplete', listener, true);
      this.runWithTab(doc);
    }.bind(this));
  }
};
