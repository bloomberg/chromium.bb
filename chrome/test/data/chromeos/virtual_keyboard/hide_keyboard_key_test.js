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
   * Mocks pressing on hide keyboard key.
   * @param {string} keysetId Initial keyset.
   */
  keyPress: function(keysetId) {
    var self = this;
    var fn = function() {
      Debug('Mock keypress on hide keyboard key.');

      // Verify that popup is initially hidden.
      var popup = self.overlay;
      assertTrue(!!popup && popup.hidden,
                 'Keyboard overlay should be hidden initially.');

      var hideKey =
          $('keyboard').activeKeyset.querySelector('kb-hide-keyboard-key');
      assertTrue(!!hideKey, 'Unable to find hide keyboard key.');
      hideKey.down({pointerId: 1});
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Mocks tap/click on hide keyboard key. It should hide and unlock keyboard.
   * @param {string} keysetId Initial keyset.
   */
  keyTap: function(keysetId) {
    var self = this;
    var fn = function() {
      Debug('Mock keypress on hide keyboard key.');

      var hideKey =
          $('keyboard').activeKeyset.querySelector('kb-hide-keyboard-key');
      assertTrue(!!hideKey, 'Unable to find hide keyboard key.');

      chrome.virtualKeyboardPrivate.hideKeyboard.addExpectation();
      // Hide keyboard should unlock keyboard too.
      chrome.virtualKeyboardPrivate.lockKeyboard.addExpectation(false);

      hideKey.down({pointerId: 1});
      hideKey.up({pointerId: 1});
    };
    this.addWaitCondition(fn, keysetId);
    this.addSubtask(fn);
  },

  /**
   * Mocks selection of lock/unlock button from the options menu.
   * @param {boolean} expect Whether or not the keyboard should be locked.
   */
  selectLockUnlockButton: function(expect) {
    var self = this;
    var fn = function() {
      Debug('mock keyup on lock/unlock button.');

      var optionsMenu = self.overlay.shadowRoot.querySelector(
          'kb-options-menu');
      assertTrue(!!optionsMenu, 'Unable to find options menu.');
      var lockUnlockButton = optionsMenu.shadowRoot.querySelector(
          'kb-options-menu-toggle-lock-item');
      assertTrue(!!lockUnlockButton, 'Unable to find lock/unlock button.');

      // should hide keyboard
      chrome.virtualKeyboardPrivate.lockKeyboard.addExpectation(expect);
      lockUnlockButton.up();
      self.overlay.up();
    };
    fn.waitCondition = {
      state: 'overlayVisibility',
      value: true
    };
    this.addSubtask(fn);
  },

  /**
   * Waits for the overlay to close.
   */
  verifyClosedOverlay: function() {
    var fn = function() {
      Debug('Validated that overlay has closed.');
    };
    fn.waitCondition = {
      state: 'overlayVisibility',
      value: false
    };
    this.addSubtask(fn);
  }
};

function testLockUnlockKeyboard(testDoneCallback) {
  var tester = new HideKeyboardKeyTester(Layout.DEFAULT);

  /**
   * Mocks type on hide keyboard key and select lock/unlock button.
   * @param {boolean} expect Whether or not the keyboard should be locked.
   */
  var checkTypeLockUnlockKey = function(expect) {
    tester.keyPress(Keyset.LOWER);
    tester.wait(1000, Keyset.LOWER);
    tester.selectLockUnlockButton(expect);
    tester.verifyClosedOverlay();
  };

  checkTypeLockUnlockKey(true);  // Expect to lock keyboard.
  checkTypeLockUnlockKey(false);  // Expect to unlock keyboard.

  checkTypeLockUnlockKey(true);  // Expect to lock keyboard.
  tester.keyTap(Keyset.LOWER);

  tester.scheduleTest('testLockUnlockKeyboard', testDoneCallback);
}
