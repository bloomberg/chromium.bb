// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sample tests that exercise the test JS library and show how this framework
// could be used to test the downloads page.
// Call this method to run the tests.
// The method takes an array of functions (your tests).
runTests([
  function testAssertFalse() {
    assertFalse(false);
  },

  function testInitialFocus() {
    assertTrue(document.activeElement.id == 'term', '');
  }
]);
