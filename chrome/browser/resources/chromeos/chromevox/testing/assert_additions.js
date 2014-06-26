// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that a given argument's value is undefined.
 * @param {object} a The argument to check.
 */
function assertUndefined(a) {
  if (a !== undefined) {
    throw new Error('Assertion failed: expected undefined');
  }
}

/**
 * Asserts that a given function call throws an exception.
 * @param {string} msg Message to print if exception not thrown.
 * @param {Function} fn The function to call.
 * @param {string} error The name of the exception we expect {@code fn} to
 *     throw.
 */
function assertException(msg, fn, error) {
  try {
    fn();
  } catch (e) {
    if (error && e.name != error) {
      throw new Error('Expected to throw ' + error + ' but threw ' + e.name +
          ' - ' + msg);
    }
    return;
  }

  throw new Error('Expected to throw exception ' + error + ' - ' + msg);
}

/**
 * Asserts that two arrays of strings are equal.
 * @param {Array.<string>} array1 The expected array.
 * @param {Array.<string>} array2 The test array.
 */
function assertEqualStringArrays(array1, array2) {
  var same = true;
  if (array1.length != array2.length) {
    same = false;
  }
  for (var i = 0; i < Math.min(array1.length, array2.length); i++) {
    if (array1[i].trim() != array2[i].trim()) {
      same = false;
    }
  }
  if (!same) {
    throw new Error('Expected ' + JSON.stringify(array1) +
                    ', got ' + JSON.stringify(array2));
  }
}

/**
 * Asserts that two objects have the same JSON serialization.
 * @param {Object} obj1 The expected object.
 * @param {Object} obj2 The actual object.
 */
function assertEqualsJSON(obj1, obj2) {
  if (!eqJSON(obj1, obj2)) {
    throw new Error('Expected ' + JSON.stringify(obj1) +
                    ', got ' + JSON.stringify(obj2));
  }
}

assertSame = assertEquals;
assertNotSame = assertNotEquals;
