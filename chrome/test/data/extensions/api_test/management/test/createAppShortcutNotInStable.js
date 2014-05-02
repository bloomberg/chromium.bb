// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function createAppShortcutNotInStable() {
    assertEq(chrome.management.createAppShortcut, undefined);
    succeed();
  }
];

chrome.test.runTests(tests);
