/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Tests longpress on the compact keyboard.
 */
function testLongpressAsync(testDoneCallback) {
  var test = function() {
    // Mock longpress 'e' and select the first alt-key.
    mockLongpress('e', ['3', 'è', 'é', 'ê','ë', 'ē'], 0);
    // TODO(rsadam@): Test cancelling longpress.
    // TODO(rsadam@): Test moving to other alt keys.
  }
  RunTest(test, testDoneCallback);
}
