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
    var view = getActiveView();
    assertTrue(!!view, 'Unable to find active view');
    assertEquals('us', view.id, 'Expecting full layout');
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
    var view = getActiveView();
    assertTrue(!!view, 'Unable to find active view');
    assertEquals('us-compact-qwerty', view.id, 'Expecting compact layout');
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

/**
 * Tests that handwriting support is disabled by default.
 */
function testHandwritingSupportAsync(testDoneCallback) {
  onKeyboardReady(function() {
    var menu = document.querySelector('.inputview-menu-view');
    assertTrue(!!menu, 'Unable to access keyboard menu');
    assertEquals('none', getComputedStyle(menu)['display'],
                 'Menu should not be visible until activated');
    mockTap(findKeyById('Menu'));
    assertEquals('block', getComputedStyle(menu)['display'],
                 'Menu should be visible once activated');
    var hwt = menu.querySelector('#handwriting');
    assertFalse(!!hwt, 'Handwriting should be disabled by default');
    testDoneCallback();
  });
}

/**
 * Validates Handwriting layout. Though handwriting is disabled for the system
 * VK, the layout is still available and useful for testing expected behavior of
 * the IME-VKs since the codebase is shared.
 */
function testHandwritingLayoutAsync(testDoneCallback) {
  var testCallback = function () {
    var menu = document.querySelector('.inputview-menu-view');
    assertEquals('none', getComputedStyle(menu).display,
                 'Menu should be hidden initially');
    mockTap(findKeyById('Menu'));
    assertFalse(menu.hidden,
                'Menu should be visible after tapping menu toggle button');
    var hwtSelect = menu.querySelector('#handwriting');
    assertTrue(!!hwtSelect, 'Handwriting should be available for testing');
    // Keysets such as hwt and emoji lazy load in order to reduce latency before
    // the virtual keyboard is shown. Wait until the load is complete to
    // continue testing.
    onKeysetReady('hwt', function() {
      onLayoutReady('handwriting', function() {
        // Tapping the handwriting selector button triggers loading of the
        // handwriting canvas. Wait for keyset switch before continuing the
        // test.
        onSwitchToKeyset('hwt', function() {
          var view = getActiveView();
          assertEquals('hwt', view.id, 'Handwriting layout is not active.');
          var hwtCanvasView = view.querySelector('#canvasView');
          assertTrue(!!hwtCanvasView, 'Unable to find canvas view');
          var candidateView = document.getElementById('candidateView');
          assertTrue(!!candidateView, 'Unable to find candidate view');
          var backButton = candidateView.querySelector(
              '.inputview-candidate-button');
          assertEquals('HANDWRITING_BACK', backButton.textContent);
          mockTap(backButton);
          assertEquals('us-compact-qwerty', getActiveView().id,
                       'compact layout is not active.');
          testDoneCallback();
        });
        mockTap(hwtSelect);
      });
    });
  };
  var config = {
    keyset: 'us.compact.qwerty',
    languageCode: 'en',
    passwordLayout: 'us',
    name: 'English',
    options: {enableHwtForTesting: true}
  };
  onKeyboardReady(testCallback, config);
}
