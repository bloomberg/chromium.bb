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

      // A keyout event should be dispatched before a keyover event.
      baseKey.out({pointerId: 1, relatedTarget: baseKey});
      altKey.over({pointerId: 1, relatedTarget: altKey});

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
    mockTypeCharacter('\t', 0x09, Modifier.NONE, 0x09);
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
    var mockEvent = { pointerId:2 };
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
