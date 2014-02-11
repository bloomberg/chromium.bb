/*
 * Copyright 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Class for testing hide keyboard key. Other than hide keyboard as the name
 * suggested, hide keyboard key can also switch keyboard layout and lock/unlock
 * keyboard.
 * @param {string=} opt_layout Optional name of the initial layout.
 * @constructor
 */
function HideKeyboardKeyTester(layout) {
  this.layout = layout;
  this.subtasks = [];
}

HideKeyboardKeyTester.prototype = {
  __proto__: SubtaskScheduler.prototype,

  /**
   * The keyboard overlay element.
   * @type {kb-keyboard-overlay}
   */
  get overlay() {
    var overlay =
        $('keyboard').shadowRoot.querySelector('kb-keyboard-overlay');
    assertTrue(!!overlay, 'Unable to find overlay container');
    return overlay;
  },

  /**
   * Mocks tap/click on hide keyboard key. It should pop up the overlay menu.
   * @param {string} keysetId Initial keyset.
   */
  tapHideKeyboard: function(keysetId) {
    var self = this;
    var mockEvent = {pointerId: 1};
    mockEvent.stopPropagation = function() {};
    var fn = function() {
      Debug('Mock tap on hide keyboard key.');

      var hideKey =
          $('keyboard').activeKeyset.querySelector('kb-hide-keyboard-key');
      assertTrue(!!hideKey, 'Unable to find hide keyboard key.');

      hideKey.down(mockEvent);
      hideKey.up(mockEvent);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
    this.verifyOverlayVisibilityChange(true);
  },

  /**
   * Mocks tap/click on the overlay menu. It should hide the overlay menu.
   * @param {string} keysetId Initial keyset.
   */
  tapOverlay: function(keysetId) {
    var mockEvent = {pointerId: 1};
    mockEvent.stopPropagation = function() {};
    var self = this;
    var fn = function() {
      Debug('Mock tap on overlay.');
      var overlay = self.overlay;
      overlay.up(mockEvent);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
    this.verifyOverlayVisibilityChange(false);
  },

  /**
   * Mocks selection of lock/unlock button from the options menu.
   * @param {string} keysetId Initial keyset.
   * @param {boolean} expect Whether or not the keyboard should be locked.
   */
  selectLockUnlockButton: function(keysetId, expect) {
    var mockEvent = {pointerId: 1};
    mockEvent.stopPropagation = function() {};
    var self = this;
    var fn = function() {
      Debug('mock keyup on lock/unlock button.');

      var optionsMenu = self.overlay.shadowRoot.querySelector(
          'kb-options-menu');
      assertTrue(!!optionsMenu, 'Unable to find options menu.');
      var lockUnlockButton = optionsMenu.shadowRoot.querySelector(
          'kb-options-menu-toggle-lock-item');
      assertTrue(!!lockUnlockButton, 'Unable to find lock/unlock button.');
      // Should toggle lock.
      chrome.virtualKeyboardPrivate.lockKeyboard.addExpectation(expect);
      lockUnlockButton.up(mockEvent);
      self.overlay.up(mockEvent);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
    this.verifyOverlayVisibilityChange(false);
  },

  /**
   * Mocks selection of hide from the options menu.
   * @param {string} keysetId Initial keyset.
   */
  selectHide: function(keysetId) {
    var mockEvent = {pointerId: 1};
    mockEvent.stopPropagation = function() {};
    var self = this;
    var fn = function() {
      Debug('mock keyup on hide button.');
      var optionsMenu = self.overlay.shadowRoot.querySelector(
          'kb-options-menu');
      assertTrue(!!optionsMenu, 'Unable to find options menu.');
      var button = optionsMenu.shadowRoot.querySelector(
          'kb-options-menu-item[layout=none]');
      assertTrue(!!button, 'Unable to find hide button.');
      // Expect hideKeyboard to be called.
      chrome.virtualKeyboardPrivate.hideKeyboard.addExpectation();
      // hideKeyboard in api_adapter also unlocks the keyboard.
      chrome.virtualKeyboardPrivate.lockKeyboard.addExpectation(false);
      button.up(mockEvent);
      self.overlay.up(mockEvent);
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
    this.verifyOverlayVisibilityChange(false);
  },

  /**
   * Waits for the overlay to change visibility.
   * @param {string} keysetId Initial keyset.
   */
  verifyOverlayVisibilityChange: function(visibility) {
    var fn = function() {
      if (visibility)
        Debug('Validated that overlay is now visible.');
      else
        Debug('Validated that overlay is now hidden.');
    };
    fn.waitCondition = {
      state: 'overlayVisibility',
      value: visibility
    };
    this.addSubtask(fn);
  }
};

/**
 * Tests that the overlay menu appears when the hide keyboard button is clicked
 * and disappears when the overlay is then clicked.
 * @param {Function} testDoneCallback The callback function on completion.
 */
function testOverlayMenu(testDoneCallback) {
  var tester = new HideKeyboardKeyTester(Layout.DEFAULT);
  tester.tapHideKeyboard(Keyset.LOWER);
  tester.tapOverlay(Keyset.LOWER);
  tester.scheduleTest('testOverlayMenu', testDoneCallback);
}

/**
 * Tests that clicking hide in the overlay menu hides the keyboard.
 * @param {Function} testDoneCallback The callback function on completion.
 */
function testHideKeyboard(testDoneCallback) {
  var tester = new HideKeyboardKeyTester(Layout.DEFAULT);
  tester.tapHideKeyboard(Keyset.LOWER);
  tester.selectHide(Keyset.LOWER);
  tester.scheduleTest('testHideKeyboard', testDoneCallback);
}

/**
 * Tests that the lock keyboard in the overlay menu toggles.
 * @param {Function} testDoneCallback The callback function on completion.
 */
function testLockKeyboard(testDoneCallback) {
  var tester = new HideKeyboardKeyTester(Layout.DEFAULT);
  tester.tapHideKeyboard(Keyset.LOWER);
  tester.selectLockUnlockButton(Keyset.LOWER, true);

  tester.tapHideKeyboard(Keyset.LOWER);
  tester.selectLockUnlockButton(Keyset.LOWER, false);

  tester.scheduleTest('testLockKeyboard', testDoneCallback);
}
