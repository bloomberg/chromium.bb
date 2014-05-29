/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Tests typing in the lowercase keyset.
 */
function testLowercaseKeysetAsync(testDoneCallback) {
  var test = function() {
    // Mouse events.
    mockMouseType('l');
    mockMouseType('p');

    // Touch events.
    mockTouchType('l');
    mockTouchType('p');
  }
  RunTest(test, testDoneCallback);
}
