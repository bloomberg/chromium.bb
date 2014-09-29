// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common testing utilities.

/**
 * Shortcut for document.getElementById.
 * @param {string} id of the element.
 * @return {HTMLElement} with the id.
 */
function $(id) {
  return document.getElementById(id);
}

/**
 * @constructor
 */
var TestUtils = function() {};

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
 * @return {string} The html text.
*/
TestUtils.extractHtmlFromCommentEncodedString = function(commentEncodedHtml) {
  return commentEncodedHtml.toString().
      replace(/^[^\/]+\/\*!?/, '').
      replace(/\*\/[^\/]+$/, '');
};
