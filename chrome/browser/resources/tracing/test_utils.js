// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helper functions for use in tracing tests.
 */


/**
 * goog.testing.assertion's assertEquals tweaked to do equality-to-a-constant.
 * @param {*} a First value.
 * @param {*} b Second value.
 */
function assertAlmostEquals(a, b) {
  _validateArguments(2, arguments);
  var var1 = nonCommentArg(1, 2, arguments);
  var var2 = nonCommentArg(2, 2, arguments);
  _assert(commentArg(2, arguments), Math.abs(var1 - var2) < 0.00001,
          'Expected ' + _displayStringForValue(var1) + ' but was ' +
          _displayStringForValue(var2));
}

cr.define('test_utils', function() {
  function getAsync(url, cb) {
    var req = new XMLHttpRequest();
    req.open('GET', url, true);
    req.onreadystatechange = function(aEvt) {
      if (req.readyState == 4) {
        window.setTimeout(function() {
          if (req.status == 200) {
            cb(req.responseText);
          } else {
            console.log('Failed to load ' + url);
          }
        }, 0);
      }
    };
    req.send(null);
  }
  return {
    getAsync: getAsync
  };
});
