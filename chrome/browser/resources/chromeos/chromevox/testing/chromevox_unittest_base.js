// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE([
    'chrome/browser/resources/chromeos/chromevox/testing/assert_additions.js']);

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * Base test fixture for ChromeVox unit tests.
 *
 * Note that while conceptually these are unit tests, these tests need
 * to run in a full web page, so they're actually run as WebUI browser
 * tests.
 *
 * @constructor
 * @extends {testing.Test}
 */
function ChromeVoxUnitTestBase() {}

ChromeVoxUnitTestBase.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  browsePreload: DUMMY_URL,

  /**
   * @override
   * It doesn't make sense to run the accessibility audit on these tests,
   * since many of them are deliberately testing inaccessible html.
   */
  runAccessibilityChecks: false,

  /**
   * Loads some inlined html into the current document, replacing
   * whatever was there previously.
   * @param {string} html The html to load as a string.
   */
  loadHtml: function(html) {
    document.open();
    document.write(html);
    document.close();
  },

  /**
   * Loads some inlined html into the current document, replacing
   * whatever was there previously. This version takes the html
   * encoded as a comment inside a function, so you can use it like this:
   *
   * this.loadDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  loadDoc: function(commentEncodedHtml) {
    var html = commentEncodedHtml.toString().
        replace(/^[^\/]+\/\*!?/, '').
        replace(/\*\/[^\/]+$/, '');
    this.loadHtml(html);
  }
};
