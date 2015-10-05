// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testDoDefault() {
    var firstTextField = findAutomationNode(rootNode,
        function(node) {
          return node.role == 'textField';
        });
    assertTrue(!!firstTextField);
    listenOnce(firstTextField, EventType.focus, function(e) {
      chrome.test.succeed();
    }, true);
    firstTextField.doDefault();
  },

  function testFocus() {
    var firstFocusableNode = findAutomationNode(rootNode,
        function(node) {
          return node.role == 'button' && node.state.focusable;
        });
    assertTrue(!!firstFocusableNode);
    listenOnce(firstFocusableNode, EventType.focus, function(e) {
      chrome.test.succeed();
    }, true);
    firstFocusableNode.focus();
  },

  function testDoDefaultViews() {
    listenOnce(rootNode, 'focus', function(node) {
      chrome.test.succeed();
    }, true);
    var button = rootNode.find(
        {role: 'button', attributes: {name: 'Bookmark this page'}});
    button.doDefault();
  },

  function testContextMenu() {
    var addressBar = rootNode.find({role: 'textField'});
    listenOnce(rootNode, EventType.menuStart, function(e) {
      addressBar.showContextMenu();
      chrome.test.succeed();
    }, true);
    addressBar.showContextMenu();
  }
];

setUpAndRunTests(allTests);
