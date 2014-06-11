// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var callbackFail = chrome.test.callbackFail;
var callbackPass = chrome.test.callbackPass;

var RoleType = chrome.automation.RoleType;

chrome.browserAction.onClicked.addListener(function() {
  chrome.tabs.executeScript({ code: 'true' }, callbackPass());
  chrome.automation.getTree(callbackPass(function(rootNode) {
    assertFalse(rootNode == undefined);
    assertEq(RoleType.rootWebArea, rootNode.role);
    chrome.test.succeed();
  }));
});

chrome.webNavigation.onCompleted.addListener(function(details) {
  chrome.tabs.executeScript({ code: 'true' }, callbackFail(
         'Cannot access contents of url "' + details.url +
         '". Extension manifest must request permission to access this host.'));

  chrome.automation.getTree(callbackFail(
      'Cannot request automation tree on url "' + details.url +
        '". Extension manifest must request permission to access this host.'));
});
