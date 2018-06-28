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

  behaviors: [
    LockStateBehavior,
  ],

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
     * Authentication token used when calling setModes, returned by
     * quickUnlockPrivate.getAuthToken. Reflected to lock-screen.
     * @private
     */
    authToken: {
      type: String,
      notify: true,
    },

    /**
     * @private
     */
    inputValue_: {
      type: Boolean,
      value: '',
      observer: 'onInputValueChange_',
    },

    /**
     * Helper property which marks password as valid/invalid.
     * @private
     */
    passwordInvalid_: {
      type: Boolean,
      value: false,
    },

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

  /** @return {!CrInputElement} */
  get passwordInput() {
    return this.$.passwordInput;
  },

  /** @override */
  attached: function() {
    this.writeUma_(LockScreenProgress.START_SCREEN_LOCK);
    this.$.dialog.showModal();
    // This needs to occur at the next paint otherwise the password input will
    // not receive focus.
    this.async(() => {
      this.passwordInput.focus();
    }, 1);
  },

  /** @private */
  onCancelTap_: function() {
    if (this.$.dialog.open)
      this.$.dialog.close();
  },

  /**
   * Run the account password check.
   * @private
   */
  submitPassword_: function() {
    clearTimeout(this.clearAccountPasswordTimeout_);

    const password = this.passwordInput.value;
    // The user might have started entering a password and then deleted it all.
    // Do not submit/show an error in this case.
    if (!password) {
      this.passwordInvalid_ = false;
      return;
    }

    this.quickUnlockPrivate.getAuthToken(password, (tokenInfo) => {
      if (chrome.runtime.lastError) {
        this.passwordInvalid_ = true;
        // Select the whole password if user entered an incorrect password.
        this.passwordInput.select();
        return;
      }

      this.authToken = tokenInfo.token;
      this.passwordInvalid_ = false;

      // Create the |this.setModes| closure and automatically clear it after
      // tokenInfo.lifetimeSeconds.
      this.setModes = (modes, credentials, onComplete) => {
        this.quickUnlockPrivate.setModes(
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
  onInputValueChange_: function() {
    this.passwordInvalid_ = false;
  },

  /** @private */
  isConfirmEnabled_: function() {
    return !this.passwordInvalid_ && this.inputValue_;
  },

  /**
   * Looks up the translation id, which depends on PIN login support.
   * @param {boolean} hasPinLogin
   * @private
   */
  selectPasswordPromptEnterPasswordString(hasPinLogin) {
    if (hasPinLogin)
      return this.i18n('passwordPromptEnterPasswordLoginLock');
    return this.i18n('passwordPromptEnterPasswordLock');
  },
});
})();
