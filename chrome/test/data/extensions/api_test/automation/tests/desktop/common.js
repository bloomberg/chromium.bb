// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

var rootNode = null;

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
  chrome.automation.getDesktop(function(rootNodeArg) {
    rootNode = rootNodeArg;
    chrome.test.runTests(allTests);
  });
}
