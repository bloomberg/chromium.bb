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
   * Loads some inlined html into the body of the current document, replacing
   * whatever was there previously.
   * @param {string} html The html to load as a string.
   */
  loadHtml: function(html) {
    while (document.head.firstChild) {
      document.head.removeChild(document.head.firstChild);
    }
    while (document.body.firstChild) {
      document.body.removeChild(document.body.firstChild);
    }
    this.appendHtml(html);
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
    var html = this.extractHtmlFromCommentEncodedString_(commentEncodedHtml);
    this.loadHtml(html);
  },

  /**
   * Appends some inlined html into the current document, at the end of
   * the body element. Takes the html encoded as a comment inside a function,
   * so you can use it like this:
   *
   * this.appendDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html to load, embedded as a
   *     comment inside an anonymous function - see example, above.
   */
  appendDoc: function(commentEncodedHtml) {
    var html = this.extractHtmlFromCommentEncodedString_(commentEncodedHtml);
    this.appendHtml(html);
  },

  /**
   * Appends some inlined html into the current document, at the end of
   * the body element.
   * @param {string} html The html to load as a string.
   */
  appendHtml: function(html) {
    var div = document.createElement('div');
    div.innerHTML = html;
    var fragment = document.createDocumentFragment();
    while (div.firstChild) {
      fragment.appendChild(div.firstChild);
    }
    document.body.appendChild(fragment);
  },

  /**
   * Extracts some inlined html encoded as a comment inside a function,
   * so you can use it like this:
   *
   * this.appendDoc(function() {/*!
   *     <p>Html goes here</p>
   * * /});
   *
   * @param {Function} commentEncodedHtml The html , embedded as a
   *     comment inside an anonymous function - see example, above.
   @ @return {String} The html text.
   */
  extractHtmlFromCommentEncodedString_: function(commentEncodedHtml) {
    return commentEncodedHtml.toString().
        replace(/^[^\/]+\/\*!?/, '').
        replace(/\*\/[^\/]+$/, '');
  }
};
