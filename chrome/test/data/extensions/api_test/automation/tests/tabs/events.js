// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testEventListenerTarget() {
    var cancelButton = tree.root.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
    cancelButton.addEventListener('focus', function onFocusTarget(event) {
      window.setTimeout(function() {
        cancelButton.removeEventListener('focus', onFocusTarget);
        chrome.test.succeed();
      }, 0);
    });
    cancelButton.focus();
  },
  function testEventListenerBubble() {
    var cancelButton = tree.root.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
    var cancelButtonGotEvent = false;
    cancelButton.addEventListener('focus', function onFocusBubble(event) {
      cancelButtonGotEvent = true;
      cancelButton.removeEventListener('focus', onFocusBubble);
    });
    tree.root.addEventListener('focus', function onFocusBubbleRoot(event) {
      assertEq('focus', event.type);
      assertEq(cancelButton, event.target);
      assertTrue(cancelButtonGotEvent);
      tree.root.removeEventListener('focus', onFocusBubbleRoot);
      chrome.test.succeed();
    });
    cancelButton.focus();
  },
  function testStopPropagation() {
    var cancelButton = tree.root.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
    function onFocusStopPropRoot(event) {
      tree.root.removeEventListener('focus', onFocusStopPropRoot);
      chrome.test.fail("Focus event was propagated to root");
    };
    cancelButton.addEventListener('focus', function onFocusStopProp(event) {
      cancelButton.removeEventListener('focus', onFocusStopProp);
      event.stopPropagation();
      window.setTimeout((function() {
        tree.root.removeEventListener('focus', onFocusStopPropRoot);
        chrome.test.succeed();
      }).bind(this), 0);
    });
    tree.root.addEventListener('focus', onFocusStopPropRoot);
    cancelButton.focus();
  },
  function testEventListenerCapture() {
    var cancelButton = tree.root.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes['ax_attr_name']);
    var cancelButtonGotEvent = false;
    function onFocusCapture(event) {
      cancelButtonGotEvent = true;
      cancelButton.removeEventListener('focus', onFocusCapture);
      chrome.test.fail("Focus event was not captured by root");
    };
    cancelButton.addEventListener('focus', onFocusCapture);
    tree.root.addEventListener('focus', function onFocusCaptureRoot(event) {
      assertEq('focus', event.type);
      assertEq(cancelButton, event.target);
      assertFalse(cancelButtonGotEvent);
      event.stopPropagation();
      tree.root.removeEventListener('focus', onFocusCaptureRoot);
      tree.root.removeEventListener('focus', onFocusCapture);
      window.setTimeout(chrome.test.succeed.bind(this), 0);
    }, true);
    cancelButton.focus();
  }
];

setUpAndRunTests(allTests)
