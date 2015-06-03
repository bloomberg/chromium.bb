// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Framework for running JavaScript tests of Polymer elements.
 */

/**
 * Test fixture for Polymer element testing.
 * @constructor
 * @extends testing.Test
 */
function PolymerTest() {
}

PolymerTest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Navigate to a WebUI to satisfy BrowserTest conditions. Override to load a
   * more useful WebUI.
   * @override
   */
  browsePreload: 'chrome://chrome-urls/',

  /**
   * The mocha adapter assumes all tests are async.
   * @override
   * @final
   */
  isAsync: true,

  /**
   * @override
   * @final
   */
  extraLibraries: ['../../../../third_party/mocha/mocha.js',
                   'mocha_adapter.js'],

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);

    // Import Polymer before running tests.
    suiteSetup(function(done) {
      var link = document.createElement('link');
      link.rel = 'import';
      link.onload = function() {
        done();
      };
      link.onerror = function() {
        done(new Error('Failed to load Polymer!'));
      };
      link.href = 'chrome://resources/polymer/v0_8/polymer/polymer.html';
      document.head.appendChild(link);
    });
  },
};

/**
 * Imports the HTML file, then calls |done| on success or throws an error.
 * @param {String} href The URL to load.
 * @param {Function} done The done callback.
 */
PolymerTest.importHref = function(href, done) {
  Polymer.Base.importHref(
      href,
      function() { done(); },
      function() { throw new Error('Failed to load ' + href); });
};

/**
 * Removes all content from the body.
 */
PolymerTest.clearBody = function() {
  document.body.innerHTML = '';
};
