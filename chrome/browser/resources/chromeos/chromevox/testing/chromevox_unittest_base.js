// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * Loads some inlined html into the current document, replacing
   * whatever was there previously. To use it, call it with the html
   * inside an inline comment, like this:
   *
   * this.loadDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   */
  loadDoc: function(commentEncodedHtml) {
    var html = commentEncodedHtml.toString().
        replace(/^[^\/]+\/\*!?/, '').
        replace(/\*\/[^\/]+$/, '');
    document.open();
    document.write(html);
    document.close();
  }
};
