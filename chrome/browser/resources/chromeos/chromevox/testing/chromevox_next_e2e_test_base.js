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
function ChromeVoxNextE2ETest() {}

ChromeVoxNextE2ETest.prototype = {
  __proto__: ChromeVoxE2ETest.prototype,

  /**
   * This method is called without |this| bound to an instance of
   * ChromeVoxNextE2ETest.
   * @override
   */
  testGenCppIncludes: function() {
    ChromeVoxE2ETest.prototype.testGenCppIncludes.call(this);
    GEN('#include "base/command_line.h"');
  },

  /**
   * Launches a new tab with the given document, and runs callback when a load
   * complete fires.
   * @param {function() : void} doc Snippet wrapped inside of a function.
   * @param {function()} opt_callback Called once the document is ready.
   */
  runWithLoadedTree: function(doc, callback) {
    chrome.automation.getDesktop(function(r) {
      function callbackInternal(evt) {
        if (!evt.target.attributes.url ||
            evt.target.attributes.url.indexOf('test') == -1)
          return;

        r.removeEventListener(callbackInternal);
        callback(evt.target);
      }
      r.addEventListener('loadComplete', callbackInternal, true);
      this.runWithTab(doc);
    }.bind(this));
  }
};
