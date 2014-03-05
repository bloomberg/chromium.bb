/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Tester for lonpress typing accents.
 */
function LongPressTypingTester(layout) {
  this.layout = layout;
  this.subtasks = [];
}

LongPressTypingTester.prototype = {
  /**
   * Extends the subtask scheduler.
   */
  __proto__: SubtaskScheduler.prototype,

  /**
   * The candidate popup.
   * @type {?kb-altkey-container}
   */
  get candidatesPopup() {
    var keyset = $('keyboard').activeKeyset;
    assertTrue(!!keyset, 'Unable to find active keyset.');
    var popup = keyset.querySelector('kb-altkey-container');
    assertTrue(!!popup, 'Unable to find altkey container.');
    return popup;
  },

  /**
   * Mocks pressing a key.
   * @param {string} label The key to press.
   * @param {string} keysetId Initial keyset.
   */
  keyPress: function(label, keysetId) {
    var self = this;
    var fn = function() {
      Debug('mock keypress on \'' + label + '\'');

      // Verify that popup is initially hidden.
      var popup = self.candidatesPopup;
      assertTrue(!!popup && popup.hidden,
                 'Candidate popup should be hidden initially.');

      // Mock keypress.
      var key = findKey(label);
      assertTrue(!!key, 'Unable to find key labelled "' + label + '".');
      key.down({pointerId: 1});
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Retrieves a key from the candidate popup.
   * @return {?kb-altkey}  The matching key or undefined if not found.
   */
  findCandidate: function(label) {
    var popup = this.candidatesPopup;
    var candidates = popup.querySelectorAll('kb-altkey');
    for (var i = 0; i < candidates.length; i++) {
      if (candidates[i].textContent == label)
        return candidates[i];
    }
  },

  /**
   * Mocks selection of a key from the candidate popup.
   * @param {string} baseLabel Label for the pressed key.
   * @param {string} candidateLabel Label for the selected key.
   * @param {int} keyCode The key code for the selected key.
   */
  selectCandidate: function(baseLabel, candidateLabel, keyCode) {
    var self = this;
    var fn = function() {
      Debug('mock keyup on \'' + candidateLabel + '\'');

      // Verify that popup window is visible.
      var popup = self.candidatesPopup;
      assertFalse(popup.hidden, 'Candidate popup should be visible.');

      // Verify that the popup window contains the base and candidate keys.
      var altKey = self.findCandidate(candidateLabel);
      var baseKey = self.findCandidate(baseLabel);
      var errorMessage = function(label) {
        return 'Unable to find \'' + label + '\' in candidate list.'
      }
      assertTrue(!!altKey, errorMessage(candidateLabel));
      assertTrue(!!baseKey, errorMessage(baseLabel));

      // A keyout event should be dispatched before a keyover event if the
      // candidateLabel is not the baseLabel.
      if (baseLabel != candidateLabel) {
        baseKey.out({pointerId: 1, relatedTarget: baseKey});
        altKey.over({pointerId: 1, relatedTarget: altKey});
      }

      // Verify that the candidate key is typed on release of the longpress.
      var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
      var unicodeValue = candidateLabel.charCodeAt(0);
      send.addExpectation({
        type: 'keydown',
        charValue: unicodeValue,
        keyCode: keyCode,
        modifiers: Modifier.NONE
      });
      send.addExpectation({
        type: 'keyup',
        charValue: unicodeValue,
        keyCode: keyCode,
        modifiers: Modifier.NONE
      });
      var mockEvent = {pointerId: 1};
      altKey.up(mockEvent);
      popup.up(mockEvent);
    };
    fn.waitCondition = {
      state: 'candidatePopupVisibility',
      value: true
    };
    this.addSubtask(fn);
  },

  /**
   * Mocks aborting a longpress selection.
   * @param {string} baselabel The label for the pressed key.
   */
  abortSelection: function(baseLabel) {
    var self = this;
    var fn = function() {
      Debug('mock abort on \'' + baseLabel + '\'');

      // Verify that popup window is visible.
      var popup = self.candidatesPopup;
      assertFalse(popup.hidden, 'Candidate popup should be visible.');

      // Verify that the popup window contains the base and candidate keys.
      var baseKey = self.findCandidate(baseLabel);
      var errorMessage = function(label) {
        return 'Unable to find \'' + label + '\' in candidate list.'
      }
      assertTrue(!!baseKey, errorMessage(baseLabel));
      baseKey.out({pointerId: 1, relatedTarget: baseKey});
      popup.up({pointerId: 1});
    };
    fn.waitCondition = {
      state: 'candidatePopupVisibility',
      value: true
    };
    this.addSubtask(fn);
  },

  /**
   * Waits for the candidate popup to close.
   */
  verifyClosedPopup: function() {
    var fn = function() {
      Debug('Validated that candidate popup has closed.');
    };
    fn.waitCondition = {
      state: 'candidatePopupVisibility',
      value: false
    };
    this.addSubtask(fn);
  }
}

/**
 * Tests that typing characters on the default lowercase keyboard triggers the
 * correct sequence of events. The test is run asynchronously since the
 * keyboard loads keysets dynamically.
 */
function testLowercaseKeysetAsync(testDoneCallback) {
  var runTest = function() {
    // Keyboard defaults to lowercase.
    mockTypeCharacter('a', 0x41, Modifier.NONE);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
    mockTypeCharacter('.', 0xBE, Modifier.NONE);
    mockTypeCharacter('\b', 0x08, Modifier.NONE, 0x08);
    mockTypeCharacter('\n', 0x0D, Modifier.NONE, 0x0A);
    mockTypeCharacter(' ', 0x20, Modifier.NONE);
  };
  onKeyboardReady('testLowercaseKeysetAsync', runTest, testDoneCallback);
}

/**
 * Tests long press on a key that has alternate sugestions. For example,
 * longpressing the 'a' key displays 'a acute' 'a grave', etc. Longpressing
 * characters on the top row of the keyboard displays numbers as alternatives.
 */
function testLongPressTypeAccentedCharacterAsync(testDoneCallback) {
  var tester = new LongPressTypingTester(Layout.DEFAULT);

  var checkLongPressType = function(key, candidate, keyCode) {
    tester.keyPress(key, Keyset.LOWER);
    tester.wait(1000, Keyset.LOWER);
    tester.selectCandidate(key, candidate, keyCode);
    tester.verifyClosedPopup();
  };

  // Test popup for letters with candidate lists that are derived from a
  // single source (hintText or accents).
  // Type lowercase A grave
  checkLongPressType('a', '\u00E0', 0);
  // Type the digit '1' (hintText on 'q' key).
  checkLongPressType('q', '1', 0x31);

  // Test popup for letter that has a candidate list combining hintText and
  // accented letters.
  // Type lowercase E acute.
  checkLongPressType('e', '\u00E9', 0, Modifier.NONE);
  // Type the digit '3' (hintText on the 'e' key).
  checkLongPressType('e', '3', 0x33, Modifier.NONE);

  // Mock aborting a longpress selection.
  tester.keyPress('e', Keyset.LOWER);
  tester.wait(1000, Keyset.LOWER);
  tester.abortSelection('e');
  tester.verifyClosedPopup();

  // Mock longpress q then release it. Catches regression in crbug/305649.
  checkLongPressType('q', 'q', 0x51);

  tester.scheduleTest('testLongPressTypeAccentedCharacterAsync',
                      testDoneCallback);
}

/**
 * When typing quickly, one can often press a second key before releasing the
 * first. This test confirms that both keys are typed in the correct order.
 */
function testAutoReleasePreviousKey(testDoneCallback) {
  var runTest = function() {
    var key = findKey('a');
    assertTrue(!!key, 'Unable to find key labelled "a".');
    var unicodeValue = 'a'.charCodeAt(0);
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    var mockEvent = {pointerId:2};
    key.down(mockEvent);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
  };
  onKeyboardReady('testAutoReleasePreviousKey', runTest, testDoneCallback);
}

/**
 * When touch typing, one can often press a key and move slightly out of the key
 * area before releasing the key. This test confirms that the key is not
 * dropped.
 */
function testFingerOutType(testDoneCallback) {
  var runTest = function() {
    var key = findKey('a');
    assertTrue(!!key, 'Unable to find key labelled "a".');
    var unicodeValue = 'a'.charCodeAt(0);
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;

    // Test finger moves out of typed key slightly before release. The key
    // should not be dropped.
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    var mockEvent = {
      pointerId:2,
      clientX: parseFloat(key.style.left),
      clientY: parseFloat(key.style.top),
    };
    key.down(mockEvent);
    key.out(mockEvent);
    // Mocks finger releases after moved out of the 'a' key.
    $('keyboard').up(mockEvent);

    // Test a second finger types on a different key before first finger
    // releases (yet moves out of the typed key). The first typed key should not
    // be dropped.
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: 0x41,
      modifiers: Modifier.NONE
    });
    key.down(mockEvent);
    key.out(mockEvent);
    mockTypeCharacter('s', 0x53, Modifier.NONE);
  };
  onKeyboardReady('testFingerOutType', runTest, testDoneCallback);
}

/**
 * Tests that touching around a key causes the key to be pressed.
 * @param {Function} testDoneCallback The callback function on completion.
 */
function testTouchFuzzing(testDoneCallback) {
  /**
   * Fuzzy types the specified key.
   * @param {string} label The character being typed.
   * @param {number} keyCode The legacy key code for the character.
   * @param {number} modifiers Indicates which if any of the shift, control and
   *     alt keys are being virtually pressed.
   * @param {number=} opt_unicode Optional unicode value for the character. Only
   *     required if it cannot be directly calculated from the label.
   */
  var fuzzyType = function(label, keyCode, modifiers, heightRatio, widthRatio,
      xOffset, yOffset, opt_unicode) {
    var key = findKey(label);
    assertTrue(!!key, 'Unable to find key labelled "' + label + '".');
    var unicodeValue = opt_unicode | label.charCodeAt(0);
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    send.addExpectation({
      type: 'keydown',
      charValue: unicodeValue,
      keyCode: keyCode,
      modifiers: modifiers
    });
    send.addExpectation({
      type: 'keyup',
      charValue: unicodeValue,
      keyCode: keyCode,
      modifiers: modifiers
    });
    var x = parseFloat(key.style.left) +
      widthRatio * parseFloat(key.style.width);
    var y = parseFloat(key.style.top) +
      heightRatio * parseFloat(key.style.height);
    var mockEvent = {
      pointerId: 1,
      clientX: x + xOffset,
      clientY: y + yOffset,
    };
    // Fake typing the key.
    var keyboard = $('keyboard');
    keyboard.down(mockEvent);
    keyboard.up(mockEvent);
  };
  /**
   * Fuzz types a set of keys.
   * @param {number} heightRatio Number in [0,1] indicating where to
   *    press on the key. 0 is the top of the key, 1 the bottom.
   * @param {number} widthRatio Number in [0,1] indicating where to
   *    press on the key. 0 is the left of the key, 1 the right.
   * @param {number} xOffset The offset to the touch position in the x-axis.
   * @param {number} yOffset The offset to the touch position in the y-axis.
   */
  var runFuzzTest = function(heightRatio, widthRatio, xOffset, yOffset) {
    // Test pressing characters.
    fuzzyType('a', 0x41, Modifier.NONE, heightRatio, widthRatio, xOffset,
        yOffset);
    fuzzyType('.', 0xBE, Modifier.NONE, heightRatio, widthRatio, xOffset,
        yOffset);
    fuzzyType('s', 0x53, Modifier.NONE, heightRatio, widthRatio, xOffset,
        yOffset);
    fuzzyType(' ', 0x20, Modifier.NONE, heightRatio, widthRatio, xOffset,
        yOffset);
    fuzzyType('\b', 0x08, Modifier.NONE, heightRatio, widthRatio, xOffset,
        yOffset, 0x08);
    fuzzyType('\n', 0x0D, Modifier.NONE, heightRatio, widthRatio, xOffset,
        yOffset, 0x0A);
  };

  /**
   * Tests various presses around the key. Assumes that the pitch is greater
   * than 2 px.
   */
  var runTest = function() {
    // Test a center press.
    runFuzzTest(0.5, 0.5, 0, 0);
    // Test corner.
    runFuzzTest(0, 0, 0, 0);
    runFuzzTest(0, 0, -1, -1);
    // Test left.
    runFuzzTest(0.5, 0, -1, 0);
    // Test right.
    runFuzzTest(0.5, 1, 1, 0);
    // Test top.
    runFuzzTest(0, 0.5, 0, -1);
    // Test bottom.
    runFuzzTest(1, 0.5, 0, 1);
  };
  onKeyboardReady('testTouchFuzzing', runTest, testDoneCallback);
}

/**
 * Tests that flicking upwards on a key with hintText types the hint text.
 * @param {Function} testDoneCallback The callback function on completion.
 */
// TODO(rsadam): Reenable when crbug.com/323211 is fixed.
function disabled_testSwipeFlick(testDoneCallback) {
  var mockEvent = function(xOffset, yOffset, target, relatedTarget) {
    var bounds = target.getBoundingClientRect();
    return {
      pointerId: 1,
      isPrimary: true,
      screenX: bounds.left + xOffset,
      // Note: Y is negative in the 'up' direction.
      screenY: bounds.bottom - yOffset,
      target: target,
      relatedTarget: relatedTarget
    };
  }
  var runTest = function() {
    var key = findKey('.');
    var send = chrome.virtualKeyboardPrivate.sendKeyEvent;
    // Test flick on the '.', expect '?' to appear.
    send.addExpectation({
      type: 'keydown',
      charValue: '?'.charCodeAt(0),
      keyCode: 0xBF,
      modifiers: Modifier.SHIFT
    });
    send.addExpectation({
      type: 'keyup',
      charValue: '?'.charCodeAt(0),
      keyCode: 0xBF,
      modifiers: Modifier.SHIFT
    });
    var height = key.clientHeight;
    var width = key.clientWidth;
    $('keyboard').down(mockEvent(0, 0, key));
    $('keyboard').move(mockEvent(0, height/2, key));
    $('keyboard').up(mockEvent(0, height/2, key));

    // Test flick that exits the keyboard area. Expect '1' to appear.
    var qKey = findKey('q');
    send.addExpectation({
      type: 'keydown',
      charValue: '1'.charCodeAt(0),
      keyCode: 0x31,
      modifiers: Modifier.NONE
    });
    send.addExpectation({
      type: 'keyup',
      charValue: '1'.charCodeAt(0),
      keyCode: 0x31,
      modifiers: Modifier.NONE
    });
    $('keyboard').down(mockEvent(width/2, height/2, qKey));
    $('keyboard').move(mockEvent(width/2, height, qKey));
    $('keyboard').out(mockEvent(width/2, 2*height, qKey, $('keyboard')));

    // Test basic long flick. Should not have any output.
    $('keyboard').down(mockEvent(0, 0, key));
    $('keyboard').move(mockEvent(0, height/2, key));
    $('keyboard').move(mockEvent(0, 2*height, key));
    $('keyboard').up(mockEvent(0, 2*height, key));

    // Test flick that crosses the original key boundary.
    send.addExpectation({
      type: 'keydown',
      charValue: '?'.charCodeAt(0),
      keyCode: 0xBF,
      modifiers: Modifier.SHIFT
    });
    send.addExpectation({
      type: 'keyup',
      charValue: '?'.charCodeAt(0),
      keyCode: 0xBF,
      modifiers: Modifier.SHIFT
    });
    var lKey = findKey('l');
    $('keyboard').down(mockEvent(0, height/2, key));
    $('keyboard').move(mockEvent(0, height, key));
    key.out(mockEvent(0, height, key, lKey));
    $('keyboard').move(mockEvent(0, height/2, lKey));
    $('keyboard').up(mockEvent(0, height/2, lKey));

    // Test long flick that crosses the original key boundary.
    $('keyboard').down(mockEvent(0, 0, key));
    $('keyboard').move(mockEvent(0, height/2, key));
    key.out(mockEvent(0, height, key, lKey));
    $('keyboard').move(mockEvent(0, height, lKey));
    $('keyboard').up(mockEvent(0, height, lKey));

    // Test composed swipe and flick. Should not have any output.
    var move = chrome.virtualKeyboardPrivate.moveCursor;
    move.addExpectation(SwipeDirection.RIGHT,
                        Modifier.CONTROL & Modifier.SHIFT);
    $('keyboard').down(mockEvent(0, 0, key));
    $('keyboard').move(mockEvent(0, height, key));
    $('keyboard').move(mockEvent(width, height, key));
    $('keyboard').up(mockEvent(width, height, key));
  };
  onKeyboardReady('testSwipeFlick', runTest, testDoneCallback);
}
