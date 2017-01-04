// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Fake implementation of chrome.quickUnlockPrivate for testing.
 */

/**
 * A couple weak pins to use for testing.
 * @const
 */
var TEST_WEAK_PINS = ['1111', '1234', '1313', '2001', '1010'];

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
    /** @type {!chrome.quickUnlockPrivate.CredentialRequirements} */
        this.credentialRequirements = {minLength: 4, maxLength: 0};
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
    },

    /**
     * @override
     * @param {!chrome.quickUnlockPrivate.QuickUnlockMode} mode
     * @param {string} credential
     * @param {function(
     *     !chrome.quickUnlockPrivate.CredentialCheck):void} onComplete
     */
    checkCredential: function(mode, credential, onComplete) {
      var message = {};
      var errors = [];
      var warnings = [];

      if (!!credential &&
          credential.length < this.credentialRequirements.minLength) {
        errors.push(chrome.quickUnlockPrivate.CredentialProblem.TOO_SHORT);
      }

      if (!!credential && this.credentialRequirements.maxLength != 0 &&
          credential.length > this.credentialRequirements.maxLength) {
        errors.push(chrome.quickUnlockPrivate.CredentialProblem.TOO_LONG);
      }

      if (!!credential && TEST_WEAK_PINS.includes(credential))
        warnings.push(chrome.quickUnlockPrivate.CredentialProblem.TOO_WEAK);

      message.errors = errors;
      message.warnings = warnings;
      onComplete(message);
    },

    /**
     * @override.
     * @param {!chrome.quickUnlockPrivate.QuickUnlockMode} mode
     * @param {function(
     *     !chrome.quickUnlockPrivate.CredentialRequirements):void onComplete
     */
    getCredentialRequirements: function(mode, onComplete) {
      onComplete(this.credentialRequirements);
    },
  };

  /** @type {!ChromeEvent} */
  FakeQuickUnlockPrivate.prototype.onActiveModesChanged = new FakeChromeEvent();

  return {FakeQuickUnlockPrivate: FakeQuickUnlockPrivate};
});
