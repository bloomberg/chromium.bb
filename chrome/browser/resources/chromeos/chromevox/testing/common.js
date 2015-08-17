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
 * @param {!Array=} opt_args Optional arguments to be substituted in the form
 *     $0, ... within the code block.
 * @return {string} The html text.
 */
TestUtils.extractHtmlFromCommentEncodedString =
    function(commentEncodedHtml, opt_args) {
  var stringified = commentEncodedHtml.toString();
  if (opt_args) {
    for (var i = 0; i < opt_args.length; i++)
      stringified = stringified.replace('$' + i, opt_args[i]);
  }
  return stringified.replace(/^[^\/]+\/\*!?/, '').replace(/\*\/[^\/]+$/, '');
};

/**
 * Similar to |TEST_F|. Generates a test for the given |testFixture|,
 * |testName|, and |testFunction|.
 * Used this variant when an |isAsync| fixture wants to temporarily mix in an
 * sync test.
 * @param {string} testFixture Fixture name.
 * @param {string} testName Test name.
 * @param {function} testFunction The test impl.
 */
function SYNC_TEST_F(testFixture, testName, testFunction) {
  TEST_F(testFixture, testName, function() {
    this.newCallback(testFunction)();
  });
}
