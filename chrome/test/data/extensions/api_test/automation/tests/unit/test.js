// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runWithModuleSystem(function(moduleSystem) {
  window.AutomationRootNode =
      moduleSystem.require('automationNode').AutomationRootNode;
});

var tests = [
  function testAutomationRootNode() {
    var root = new AutomationRootNode();
    chrome.test.assertTrue(root.isRootNode);
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
