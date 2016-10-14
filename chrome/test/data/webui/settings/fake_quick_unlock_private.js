// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.quickUnlockPrivate for testing.
 */
cr.define('settings', function() {
  /**
   * Fake of the chrome.quickUnlockPrivate API.
   * @constructor
   * @implements {QuickUnlockPrivate}
   */
  function FakeQuickUnlockPrivate() {
    /** @type {!Array<!chrome.quickUnlockPrivate.QuickUnlockMode>} */
        this.availableModes = [chrome.quickUnlockPrivate.QuickUnlockMode.PIN];
    /** @type {!Array<!chrome.quickUnlockPrivate.QuickUnlockMode>} */
        this.activeModes = [];
    /** @type {!Array<string>} */ this.credentials = [];
    /** @type {string} */ this.accountPassword = '';
  }

  FakeQuickUnlockPrivate.prototype = {
    // Public testing methods.
    /**
     * @override
     * @param {function(
     *     !Array<!chrome.quickUnlockPrivate.QuickUnlockMode>):void} onComplete
     */
    getAvailableModes: function(onComplete) {
      onComplete(this.availableModes);
    },

    /**
     * @override
     * @param {function(
     *     !Array<!chrome.quickUnlockPrivate.QuickUnlockMode>):void} onComplete
     */
    getActiveModes: function(onComplete) {
      onComplete(this.activeModes);
    },

    /**
     * @override
     * @param {string} accountPassword
     * @param {!Array<!chrome.quickUnlockPrivate.QuickUnlockMode>} modes
     * @param {!Array<string>} credentials
     * @param {function(boolean):void} onComplete
     */
    setModes: function(accountPassword, modes, credentials, onComplete) {
      // Even if the account password is wrong we still update activeModes and
      // credentials so that the mock owner has a chance to see what was given
      // to the API.
      this.activeModes = modes;
      this.credentials = credentials;
      onComplete(this.accountPassword == accountPassword);
    }
  };

  /** @type {!ChromeEvent} */
  FakeQuickUnlockPrivate.prototype.onActiveModesChanged = new FakeChromeEvent();

  return {FakeQuickUnlockPrivate: FakeQuickUnlockPrivate};
});
