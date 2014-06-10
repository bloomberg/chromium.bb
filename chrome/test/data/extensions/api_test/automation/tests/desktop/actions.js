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
    firstTextField.addEventListener(EventType.focus, function(e) {
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
    firstFocusableNode.addEventListener(EventType.focus, function(e) {
      chrome.test.succeed();
    }, true);
    firstFocusableNode.focus();
  }
];

setupAndRunTests(allTests);
