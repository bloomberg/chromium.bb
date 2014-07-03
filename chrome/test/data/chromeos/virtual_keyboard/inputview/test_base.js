/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var mockController;
var mockTimer;
var realSetTimeout;
var sendMessage;
var DEFAULT_CONTEXT_ID = 1;
var LONGPRESS_DELAY = 1100;

/**
 * The enumeration of message types. This should be kept in sync with the
 * InputView enums.
 * @const
 * @enum {number}
 */
var Type = {
  COMMIT_TEXT: 7,
};


/**
 * Create mocks for the virtualKeyboardPrivate API. Any tests that trigger API
 * calls must set expectations for call signatures.
 */
function setUp() {
  realSetTimeout = window.setTimeout;
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
  sendMessage = chrome.runtime.sendMessage;
  chrome.runtime.sendMessage = function(msg){
    // Forward message to the mocked method.
    if (msg.type == Type.COMMIT_TEXT)
      chrome.input.ime.commitText(msg)
    else
      console.error("Unknown message type: " + msg.type);
  };
  chrome.input.ime.commitText.validateCall = validateCommit;
}

function RunTest(testFn, testDoneCallback) {
  var pollTillReady = function() {
    if (window.isKeyboardReady()) {
      testFn();
      testDoneCallback();
    } else {
      window.startTest();
      realSetTimeout(pollTillReady, 100);
    }
  }
  pollTillReady();
}

/**
 * Verify that API calls match expectations.
 */
function tearDown() {
  mockController.verifyMocks();
  mockController.reset();
  chrome.runtime.sendMessage = sendMessage;
  mockTimer.uninstall();
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
   * in GoogleKeyboardInput-xkb.crx. Ids that start with a number are escaped
   * so that document.querySelector returns the correct element.
   */
  var KEY_IDS = {
    'a' : {
      'us' : '#\\31 01kbd-k-29',
      'us.compact.qwerty' : '#compactkbd-k-key-11',
    },
    'c' : {
      'us' : '#\\31 01kbd-k-44',
      'us.compact.qwerty' : '#compactkbd-k-key-24',
    },
    'd' : {
      'us' : '#\\31 01kbd-k-31',
      'us.compact.qwerty' : '#compactkbd-k-key-13',
    },
    'e' : {
      'us' : '#\\31 01kbd-k-17',
      'us.compact.qwerty': '#compactkbd-k-key-2',
    },
    'l' : {
      'us' : '#\\31 01kbd-k-37',
      'us.compact.qwerty' : '#compactkbd-k-key-19',
    },
    'p' : {
      'us' : '#\\31 01kbd-k-24',
      'us.compact.qwerty' : '#compactkbd-k-key-9',
    },
    'leftshift' : {
      'us' : '#\\31 01kbd-k-41',
      'us.compact.qwerty' : '#compactkbd-k-21',
    },
    "capslock" : {
      'us' : '#\\31 01kbd-k-28',
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
   * Returns the layout with the id provided. Periods in the id are replaced by
   * hyphens.
   * @param id {string} id The layout id.
   * @return {object}
   */

  var getLayout_ = function(id) {
    // Escape periods to hyphens.
    var layoutId = id.replace(/\./g, '-');
    var layout = document.querySelector('#' + layoutId);
    assertTrue(!!layout, "Cannot find layout with id: " + layoutId);
    return layout;
  }

  /**
   * Returns the layout with the id provided. Periods in the id are replaced by
   * hyphens.
   * @param id {string} id The layout id.
   * @return {object}
   */

  var getLayout_ = function(id) {
    // Escape periods to hyphens.
    var layoutId = id.replace(/\./g, '-');
    var layout = document.querySelector('#' + layoutId);
    assertTrue(!!layout, "Cannot find layout with id: " + layoutId);
    return layout;
  }

  /**
   * Returns the key object corresponding to the character.
   * @return {string} char The character.
   */
  var getKey_ = function(char) {
    var layoutId = getLayoutId();
    var layout = getLayout_(layoutId);

    // All keys in the layout that are in the target keys position.
    var candidates = layout.querySelectorAll(getKeyId_(layoutId, char));
    assertTrue(candidates.length > 0, "Cannot find key: " + char);
    var visible = Array.prototype.filter.call(candidates, isActive);

    assertEquals(1, visible.length,
        "Expect exactly one visible key for char: " + char);
    return visible[0];
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
  // UIEvent does not allow mocking pageX pageY of the event, so we use the
  // parent Event class.
  var e = document.createEvent('Event');
  e.initEvent(type, true, true);
  var rect = target.getBoundingClientRect();
  e.pageX = rect.left;
  e.pageY = rect.top;
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

/**
 * Returns, if present, the active alternate key container.
 * @return {?Object}
 */
function getActiveAltContainer() {
  // TODO(rsadam): Simplify once code refactor to remove unneeded containers is
  // complete.
  var all = document.querySelectorAll('.inputview-altdata-view');
  var filtered = Array.prototype.filter.call(all, isActive);
  assertTrue(filtered.length <= 1, "More than one active container.");
  return filtered.length > 0 ? filtered[0] : null;
}

/**
 * Mocks a character long press.
 * @param {String} char The character to longpress.
 * @param {Array<string>} altKeys the expected alt keys.
 * @param {number} index The index of the alt key to select.
 */
function mockLongpress(char, altKeys, index) {
  var key = getKey(char);

  generateTouchEvent(key, 'touchstart');
  mockTimer.tick(LONGPRESS_DELAY);

  var container = getActiveAltContainer();
  assertTrue(!!container, "Cannot find active alt container.");
  var options = container.querySelectorAll('.inputview-altdata-key');
  assertEquals(altKeys.length, options.length,
      "Unexpected number of alt keys.");
  // Check all altKeys present and in order specified.
  for (var i = 0; i < altKeys.length; i++) {
    assertEquals(altKeys[i], options[i].textContent);
  }
  // Expect selection to be typed
  var send = chrome.input.ime.commitText;
  send.addExpectation({
    contextId: DEFAULT_CONTEXT_ID,
    text: altKeys[index],
  });
  // TODO(rsadam:) Add support for touch move.
  generateTouchEvent(key, 'touchend', true, true)
  assertFalse(isActive(container), "Alt key container was not hidden.");
}
