// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testEventListenerTarget() {
    var cancelButton = rootNode.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes.name);
    cancelButton.addEventListener(EventType.focus,
                                  function onFocusTarget(event) {
      window.setTimeout(function() {
        cancelButton.removeEventListener(EventType.focus, onFocusTarget);
        chrome.test.succeed();
      }, 0);
    });
    cancelButton.focus();
  },
  function testEventListenerBubble() {
    var cancelButton = rootNode.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes.name);
    var cancelButtonGotEvent = false;
    cancelButton.addEventListener(EventType.focus,
                                  function onFocusBubble(event) {
      cancelButtonGotEvent = true;
      cancelButton.removeEventListener(EventType.focus, onFocusBubble);
    });
    rootNode.addEventListener(EventType.focus,
                               function onFocusBubbleRoot(event) {
      assertEq('focus', event.type);
      assertEq(cancelButton, event.target);
      assertTrue(cancelButtonGotEvent);
      rootNode.removeEventListener(EventType.focus, onFocusBubbleRoot);
      chrome.test.succeed();
    });
    cancelButton.focus();
  },
  function testStopPropagation() {
    var cancelButton = rootNode.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes.name);
    function onFocusStopPropRoot(event) {
      rootNode.removeEventListener(EventType.focus, onFocusStopPropRoot);
      chrome.test.fail("Focus event was propagated to root");
    };
    cancelButton.addEventListener(EventType.focus,
                                  function onFocusStopProp(event) {
      cancelButton.removeEventListener(EventType.focus, onFocusStopProp);
      event.stopPropagation();
      window.setTimeout((function() {
        rootNode.removeEventListener(EventType.focus, onFocusStopPropRoot);
        chrome.test.succeed();
      }).bind(this), 0);
    });
    rootNode.addEventListener(EventType.focus, onFocusStopPropRoot);
    cancelButton.focus();
  },
  function testEventListenerCapture() {
    var cancelButton = rootNode.firstChild().children()[2];
    assertEq('Cancel', cancelButton.attributes.name);
    var cancelButtonGotEvent = false;
    function onFocusCapture(event) {
      cancelButtonGotEvent = true;
      cancelButton.removeEventListener(EventType.focus, onFocusCapture);
      chrome.test.fail("Focus event was not captured by root");
    };
    cancelButton.addEventListener(EventType.focus, onFocusCapture);
    rootNode.addEventListener(EventType.focus,
                               function onFocusCaptureRoot(event) {
      assertEq('focus', event.type);
      assertEq(cancelButton, event.target);
      assertFalse(cancelButtonGotEvent);
      event.stopPropagation();
      rootNode.removeEventListener(EventType.focus, onFocusCaptureRoot);
      rootNode.removeEventListener(EventType.focus, onFocusCapture);
      window.setTimeout(chrome.test.succeed.bind(this), 0);
    }, true);
    cancelButton.focus();
  }
];

setUpAndRunTests(allTests)
