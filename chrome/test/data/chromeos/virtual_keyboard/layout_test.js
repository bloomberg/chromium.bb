/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Verifies that the layout matches with expectations.
 * @param {Array.<string>} rows List of strings where each string indicates the
 *     expected sequence of characters on the corresponding row.
 */
function verifyLayout(rows) {
  var rowIndex = 1;
  rows.forEach(function(sequence) {
    var rowId = 'row' + rowIndex++;
    var first = sequence[0];
    var key = findKey(first, rowId);
    assertTrue(!!key, 'Unable to find "' + first + '" in "' + rowId + '"');
    for (var i = 1; i < sequence.length; i++) {
      var next = key.nextSibling;
      assertTrue(!!next,
                 'Unable to find key to right of "' + sequence[i - 1] + '"');
      assertTrue(hasLabel(next, sequence[i]),
                 'Unexpected label: expected: "' + sequence[i] +
                 '" to follow "' + sequence[i - 1] + '"');
      key = next;
    }
  });
}

/**
 * Validates full layout for a US QWERTY keyboard.
 */
function testFullQwertyLayoutAsync(testDoneCallback) {
  var testCallback = function() {
    var lowercase = [
      '`1234567890-=',
      'qwertyuiop[]\\',
      'asdfghjkl;\'',
      'zxcvbnm,./'
    ];
    var uppercase = [
      '~!@#$%^&*()_+',
      'QWERTYUIOP{}',
      'ASDFGHJKL:"',
      'ZXCVBNM<>?'
    ];
    verifyLayout(lowercase);
    mockTap(findKeyById('ShiftLeft'));
    verifyLayout(uppercase);
    mockTap(findKeyById('ShiftRight'));
    verifyLayout(lowercase);
    testDoneCallback();
  };
  var config = {
    keyset: 'us',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English'
  };
  onKeyboardReady(testCallback, config);
}

/**
 * Validates compact layout for a US QWERTY keyboard.
 */
function testCompactQwertyLayoutAsync(testDoneCallback) {
  var testCallback = function() {
    var lowercase = [
      'qwertyuiop',
      'asdfghjkl',
      'zxcvbnm!?'
    ];
    var uppercase = [
      'QWERTYUIOP',
      'ASDFGHJKL',
      'ZXCVBNM!?'
    ];
    var symbol = [
      '1234567890',
      '@#$%&-+()',
      '\\=*"\':;!?'
    ];
    var more = [
      '~`|',
      '\u00a3\u00a2\u20ac\u00a5^\u00b0={}',
      '\\\u00a9\u00ae\u2122\u2105[]\u00a1\u00bf'
    ];
    verifyLayout(lowercase);
    mockTap(findKeyById('ShiftLeft'));
    verifyLayout(uppercase);
    mockTap(findKey('?123'));
    verifyLayout(symbol);
    mockTap(findKey('~[<'));
    verifyLayout(more);
    mockTap(findKey('abc'));
    verifyLayout(lowercase);
    testDoneCallback();
  };
  var config = {
    keyset: 'us.compact.qwerty',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English'
  };
  onKeyboardReady(testCallback, config);
}
