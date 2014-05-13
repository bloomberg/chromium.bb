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
* Checks whether the element is currently being displayed on screen.
* @param {Object} The object to check.
* @return {boolean}
*/
function isActive(el) {
  return window.getComputedStyle(el).display != "none";
}

(function(exports) {

  /**
   * Map from keys to layout specific key ids. This only contains a small subset
   * of the keys which are used in testing. The ids are based on the XKB layouts
   * in GoogleKeyboardInput-xkb.crx.
   */
  var KEY_IDS = {
    'a' : {
      'us' : '101kbd-k-29',
      'us.compact' : 'compactkbd-k-key-10',
    },
    'c' : {
      'us' : '101kbd-k-44',
      'us.compact' : 'compactkbd-k-key-21',

    },
    'd' : {
      'us' : '101kbd-k-31',
      'us.compact' : 'compactkbd-k-key-12',

    },
    'l' : {
      'us' : '101kbd-k-37',
      'us.compact' : 'compactkbd-k-key-18',

    },
    'p' : {
      'us' : '101kbd-k-24',
      'us.compact' : 'compactkbd-k-key-9',
    },
    'leftshift' : {
      'us' : '101kbd-k-41',
      'us.compact' : 'compactkbd-k-21',
    },
    "capslock" : {
      'us' : '101kbd-k-28',
    }
  };

  /**
   * Gets the key id of the specified character.
   * @param {string} layout The current keyboard layout.
   * @param {char} char The character to press.
   */
  var getKeyId_ = function(layout, char) {
    var lower = char.toLowerCase();
    assertTrue(!!KEY_IDS[lower], "Cannot find cached key id: " + char);
    assertTrue(!!KEY_IDS[lower][layout],
        "Cannot find cached key id: " + char + " in " + layout);
    return KEY_IDS[lower][layout];
  }

  /**
   * Returns the current layout id.
   * @return {string}
   */
  var getLayoutId_ = function() {
    // TODO(rsadam@): Generalize this.
    var id = window.location.search.split("id=")[1];
    assertTrue(!!id, "No layout found.");
    return id;
  }

  /**
   * Returns the key object corresponding to the character.
   * @return {string} char The character.
   */
  var getKey_ = function(char) {
    var layoutId = getLayoutId();
    var key = document.getElementById(getKeyId_(layoutId, char));
    assertTrue(!!key, "Key not present in layout: " + char);
    return key;
  }

  exports.getKey = getKey_;
  exports.getLayoutId = getLayoutId_;
})(this);

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
 * Mocks a key type using the mouse.
 * @param {Object} key The key to click on.
 */
function mockMouseTypeOnKey(key) {
  generateMouseEvent(key, 'mouseover');
  generateMouseEvent(key, 'mousedown');
  generateMouseEvent(key, 'mouseup');
  generateMouseEvent(key, 'click');
  generateMouseEvent(key, 'mouseover');
  generateMouseEvent(key, 'mouseout');
}

/**
 * Mocks a character type using the mouse. Expects the character will be
 * committed.
 * @param {String} char The character to type.
 */
function mockMouseType(char) {
  var send = chrome.input.ime.commitText;
  send.addExpectation({
    contextId: DEFAULT_CONTEXT_ID,
    text: char,
  });
  var key = getKey(char);
  mockMouseTypeOnKey(key);
}

/**
 * Generates a touch event and dispatches it on the target.
 * @param target {Object} The target of the event.
 * @param type {String} The type of the touch event.
 */
function generateTouchEvent(target, type) {
  var e = document.createEvent('UIEvents');
  e.initEvent(type, true, true);
  target.dispatchEvent(e);
}

/**
 * Mocks a character type using touch.
 * @param {String} char The expected character.
 */
function mockTouchType(char) {
  var send = chrome.input.ime.commitText;
  send.addExpectation({
    contextId: DEFAULT_CONTEXT_ID,
    text: char,
  });
  var key = getKey(char);
  generateTouchEvent(key, 'touchstart');
  generateTouchEvent(key, 'touchend');
}
