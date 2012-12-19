// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts the value is true.
 * @param {*} x The value to assert.
 */
function assert(x) {
  if (!x)
    throw new Error();
}

/**
 * Asserts the values are equal.
 * @param {*} x The value to assert.
 */
function assertEquals(x, y) {
  if (x != y)
    throw new Error(x + ' != ' + y);
}

/**
 * Runs the given test and returns whether it passed.
 * @param {function()} test The test to run.
 * @return Whether the test passed.
 */
function runTest(test) {
  try {
    test();
  } catch (e) {
    console.log('Failed ' + test.name);
    console.log(e.stack);
    return false;
  }
  return true;
}

/**
 * Runs all tests and reports the results via the console.
 */
function runTests() {
  var count = 0;
  for (var i in window) {
    if (i.indexOf('test') == 0) {
      count++;
      console.log('Running ', i);
      if (!runTest(window[i]))
        throw new Error('Test failure. Not running subsequent tests.');
    }
  }
  console.log('All %d tests passed.', count);
}

window.addEventListener('load', function() {
  runTests();
});
