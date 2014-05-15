// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testGetDesktop() {
    chrome.automation.getDesktop(function(tree) {
      tree.addEventListener('loadComplete', function(e) {
        assertEq('desktop', tree.root.role);
        assertEq('window', tree.root.firstChild().role);
        chrome.test.succeed();
      }, true);
    });
  }
];

chrome.test.runTests(allTests);
