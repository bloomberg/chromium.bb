// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var tree = null;

function findAutomationNode(root, condition) {
  if (condition(root))
    return root;

  var children = root.children();
  for (var i = 0; i < children.length; i++) {
    var result = findAutomationNode(children[i], condition);
    if (result)
      return result;
  }
  return null;
}

function setupAndRunTests(allTests) {
  chrome.automation.getDesktop(function(treeArg) {
    tree = treeArg;
    chrome.test.runTests(allTests);
  });
}
