// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 *
 * 'settings-password-prompt-dialog' shows a dialog which asks for the user to
 * enter their password. It validates the password is correct. Once the user has
 * entered their account password, the page fires an 'authenticated' event and
 * updates the setModes binding.
 *
 * The setModes binding is a wrapper around chrome.quickUnlockPrivate.setModes
 * which has a prebound account password. The account password by itself is not
 * available for other elements to access.
 *
 * Example:
 *
 * <settings-password-prompt-dialog
 *   id="passwordPrompt"
 *   set-modes="[[setModes]]">
 * </settings-password-prompt-dialog>
 *
 * this.$.passwordPrompt.open()
 */

(function() {
'use strict';

Polymer({
  is: 'settings-password-prompt-dialog',

  properties: {
    /**
     * A wrapper around chrome.quickUnlockPrivate.setModes with the account
     * password already supplied. If this is null, the authentication screen
     * needs to be redisplayed. This property will be cleared after the timeout
     * returned by quickUnlockPrivate.getAuthToken.
     * @type {?Object}
     */
    setModes: {
      type: Object,
      notify: true,
    },

    /**
     * Authhentication token used when calling setModes, returned by
     * quickUnlockPrivate.getAuthToken.
     * @private
     */
    token_: String,

    /**
     * Helper property which marks password as valid/invalid.
     * @private
     */
    passwordInvalid_: Boolean,

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overriden by tests.
     * @private {QuickUnlockPrivate}
     */
    quickUnlockPrivate_: {type: Object, value: chrome.quickUnlockPrivate},

    /**
     * writeUma_ is a function that handles writing uma stats. It may be
     * overridden for tests.
     *
     * @type {Function}
     * @private
     */
    writeUma_: {
      type: Object,
      value: function() {
        return settings.recordLockScreenProgress;
      }
    },
  },

  /** @override */
  attached: function() {
    this.writeUma_(LockScreenProgress.START_SCREEN_LOCK);
    this.$.dialog.showModal();
    this.async(() => {
      this.$.passwordInput.focus();
    });
  },

  /** @private */
  onCancelTap_: function() {
    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * Called whenever the dialog is closed.
   * @private
   */
  onClose_: function() {
    this.token_ = '';
  },

  /**
   * Run the account password check.
   * @private
   */
  submitPassword_: function() {
    clearTimeout(this.clearAccountPasswordTimeout_);

    let password = this.$.passwordInput.value;
    // The user might have started entering a password and then deleted it all.
    // Do not submit/show an error in this case.
    if (!password) {
      this.passwordInvalid_ = false;
      return;
    }

    this.quickUnlockPrivate_.getAuthToken(password, (tokenInfo) => {
      if (chrome.runtime.lastError) {
        this.passwordInvalid_ = true;
        // Select the whole password if user entered an incorrect password.
        // Return focus to the password input if it lost focus while being
        // checked (user pressed confirm button).
        this.$.passwordInput.inputElement.select();
        if (!this.$.passwordInput.focused)
          this.$.passwordInput.focus();
        return;
      }

      this.passwordInvalid_ = false;

      // Create the |this.setModes| closure and automatically clear it after
      // tokenInfo.lifetimeSeconds.
      this.setModes = (modes, credentials, onComplete) => {
        this.quickUnlockPrivate_.setModes(
            tokenInfo.token, modes, credentials, () => {
              let result = true;
              if (chrome.runtime.lastError) {
                console.error(
                    'setModes failed: ' + chrome.runtime.lastError.message);
                result = false;
              }
              onComplete(result);
            });
      };

      // Subtract time from the exiration time to account for IPC delays.
      // Treat values less than the minimum as 0 for testing.
      const IPC_SECONDS = 2;
      const lifetimeMs = tokenInfo.lifetimeSeconds > IPC_SECONDS ?
          (tokenInfo.lifetimeSeconds - IPC_SECONDS) * 1000 :
          0;
      this.clearAccountPasswordTimeout_ = setTimeout(() => {
        this.setModes = null;
      }, lifetimeMs);

      if (this.$.dialog.open)
        this.$.dialog.close();

      this.writeUma_(LockScreenProgress.ENTER_PASSWORD_CORRECTLY);
    });
  },

  /** @private */
  onPasswordChanged_: function() {
    this.passwordInvalid_ = false;
  },

  /** @private */
  enableConfirm_: function() {
    return !!this.$.passwordInput.value && !this.passwordInvalid_;
  },
});
})();
