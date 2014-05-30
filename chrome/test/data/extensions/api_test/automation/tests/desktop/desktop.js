// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testGetDesktop() {
    chrome.automation.getDesktop(function(tree) {
      assertEq(RoleType.desktop, tree.root.role);
      assertEq(RoleType.window, tree.root.firstChild().role);
      chrome.test.succeed();
    });
  },

  function testGetDesktopTwice() {
    var desktop = null;
    chrome.automation.getDesktop(function(tree) {
      desktop = tree;
    });
    chrome.automation.getDesktop(function(tree) {
      assertEq(tree, desktop);
      chrome.test.succeed();
    });
  },

  function testGetDesktopNested() {
    var desktop = null;
    chrome.automation.getDesktop(function(tree) {
      desktop = tree;
      chrome.automation.getDesktop(function(tree2) {
        assertEq(tree2, desktop);
        chrome.test.succeed();
      });
    });
  }
];

chrome.test.runTests(allTests);
