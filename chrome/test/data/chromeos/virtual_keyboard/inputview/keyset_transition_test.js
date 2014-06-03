/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

function testShiftHighlightAsync(testDoneCallback) {
  var test = function() {
    // Start in lower case.
    mockTouchType('l');
    var shift = getKey("leftShift");
    generateTouchEvent(shift, 'touchstart', true, true);
    generateTouchEvent(shift, 'touchend', true, true);
    // Transitioned to upper case.
    mockTouchType('A');
    // Should revert to lower case.
    mockTouchType('p');
    // Should remain in lower case.
    mockTouchType('c');
  }
  RunTest(test, testDoneCallback);
}

function testCapslockAsync(testDoneCallback) {
  // Skip this test for compact layouts since they don't have capslock keys.
  var id = getLayoutId();
  if (id.indexOf("compact") > 0) {
    testDoneCallback(false);
    return;
  }
  var test = function() {
    // Start in lower case.
    mockTouchType('l');
    // To upper case.
    // TODO(rsadam@): Only test this for the full layout.
    var caps = getKey("capslock")
    generateTouchEvent(caps, 'touchstart', true, true);
    generateTouchEvent(caps, 'touchend', true, true);
    mockTouchType('A');
    // Should persist upper case.
    mockTouchType('P');
    mockTouchType('C');
    // Back to lower case.
    generateTouchEvent(caps, 'touchstart', true, true);
    generateTouchEvent(caps, 'touchend', true, true);
    mockTouchType('p');
    // Persist lower case.
    mockTouchType('c')
    mockTouchType('d')

    // Same test, but using mouse events.
    // Start in lower case.
    mockMouseType('l');
    // To upper case.
    mockMouseTypeOnKey(caps);
    mockMouseType('A');
    // Should persist upper case.
    mockMouseType('P');
    mockMouseType('C');
    // Back to lower case.
    mockMouseTypeOnKey(caps);
    mockMouseType('p');
    // Persist lower case.
    mockMouseType('c')
    mockMouseType('d')
  }
  RunTest(test, testDoneCallback);
}
