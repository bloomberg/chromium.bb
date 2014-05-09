/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var mockController;
var mockTimer;
var setComposition;

var DEFAULT_CONTEXT_ID = 1;

/**
 * Create mocks for the virtualKeyboardPrivate API. Any tests that trigger API
 * calls must set expectations for call signatures.
 */
function setUp() {
  mockController = new MockController();
  mockTimer = new MockTimer();

  mockTimer.install();
  mockController.createFunctionMock(chrome.input.ime, 'commitText');

  var validateCommit = function(index, expected, observed) {
    // Only consider the first argument, the details object.
    var expectedEvent = expected[0];
    var observedEvent = observed[0];
    assertEquals(expectedEvent.text,
                 observedEvent.text,
                 'Mismatched commit text.');
  };
  chrome.input.ime.commitText.validateCall = validateCommit;
  setComposition = chrome.input.ime.setComposition;
  // Mocks setComposition manually to immediately callback. The mock controller
  // does not support callback functions.
  chrome.input.ime.setComposition = function(obj, callback) {
    callback();
  }
  window.setContext({'contextID': DEFAULT_CONTEXT_ID, 'type': 'text'});
  // TODO(rsadam): Mock additional extension API calls as required.
}

/**
 * Verify that API calls match expectations.
 */
function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
  mockTimer.uninstall();
  chrome.input.ime.setComposition = setComposition;
}

/**
 * Retrieves the key from the current keyset.
 * @param {String} char The character of the key.
 * @return {Object} The key.
 */
function getKey(char) {
  return document.querySelector('#Key' + char.toUpperCase())
}

/**
 * Generates a mouse event and dispatches it on the target.
 * @param target {Object} The target of the event.
 * @param type {String} The type of the mouse event.
 */
function generateMouseEvent(target, type) {
  var e = new MouseEvent(type, {bubbles:true, cancelable:true});
  target.dispatchEvent(e);
}

/**
 * Mocks a character type using the mouse.
 * @param {String} char The character to type.
 */
function mockMouseType(char) {
  var send = chrome.input.ime.commitText;
  send.addExpectation({
    contextId: DEFAULT_CONTEXT_ID,
    text: char,
  });
  var key = getKey(char);
  if (!key) {
    console.error("Cannot find key: " + char);
    return;
  }
  generateMouseEvent(key, 'mouseover');
  generateMouseEvent(key, 'mousedown');
  generateMouseEvent(key, 'mouseup');
  generateMouseEvent(key, 'click');
  generateMouseEvent(key, 'mouseover');
  generateMouseEvent(key, 'mouseout');
}
