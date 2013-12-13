/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

 /**
 * Asynchronously tests that invert propagates correctly. This catches the
 * regression crbug.com/3277551.
 * @param {function} testDoneCallBack The callback function to be called
 * on completion.
 */
function testInvertAttributeAsync(testDoneCallback) {
  var tester = new SubtaskScheduler();
  tester.init = function() {
    $('keyboard').layout = Layout.SYSTEM;
  };

  var onSystemQwertyLower = function() {
    assertEquals(Keyset.SYSTEM_LOWER, $('keyboard').activeKeysetId,
                 'Invalid keyset.');
    var key = findKey('[');
    assertFalse(key.invert, "Unexpected inverted key in lower keyset.");
    $('keyboard').keyset = Keyset.UPPER;
  };

  tester.addWaitCondition(onSystemQwertyLower, Keyset.SYSTEM_LOWER);
  tester.addSubtask(onSystemQwertyLower);

  var onSystemQwertyUpper = function() {
    assertEquals(Keyset.SYSTEM_UPPER, $('keyboard').activeKeysetId,
        "Invalid keyset.");
    var key = findKey('{');
    assertTrue(key.invert, "Key not inverted in upper keyset.");
    $('keyboard').layout = Layout.DEFAULT;
  }

  tester.addWaitCondition(onSystemQwertyUpper, Keyset.SYSTEM_UPPER);
  tester.addSubtask(onSystemQwertyUpper);

  var onReset = function() {
    assertEquals(Keyset.DEFAULT_LOWER, $('keyboard').activeKeysetId,
                 'Invalid keyset.');
  };
  tester.addWaitCondition(onReset, Keyset.DEFAULT_LOWER);
  tester.addSubtask(onReset);

  tester.scheduleTest('testInvertAttributeAsync', testDoneCallback);
}