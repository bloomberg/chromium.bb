// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Sample tests that exercise the test JS library and show how this framework
// could be used to test the downloads page.
var helper = {
  helpFunc: function helpFunc() {
    assertTrue(true);
  }
};

function testAssertFalse() {
  assertFalse(false);
}

function testHelper() {
  helper.helpFunc(false);
}

function testAssertTrue() {
  assertTrue(true);
}

function testAssertEquals() {
  assertEquals(5, 5, "fives");
}
