// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testDoDefault() {
    var firstTextField = findAutomationNode(tree.root,
        function(node) {
          return node.role == 'textField';
        });
    assertTrue(!!firstTextField);
    firstTextField.addEventListener('focus', function(e) {
      chrome.test.succeed();
    }, true);
    firstTextField.doDefault();
  },

  function testFocus() {
    var firstFocusableNode = findAutomationNode(tree.root,
        function(node) {
          return node.role == 'button' && node.state.focusable;
        });
    assertTrue(!!firstFocusableNode);
    firstFocusableNode.addEventListener('focus', function(e) {
      chrome.test.succeed();
    }, true);
    firstFocusableNode.focus();
  }
];

setupAndRunTests(allTests);
