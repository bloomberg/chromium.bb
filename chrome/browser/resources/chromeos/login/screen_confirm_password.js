// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Password confirmation screen implementation.
 */

login.createScreen('ConfirmPasswordScreen', 'confirm-password', function() {
  return {
    EXTERNAL_API: [
      'show'
    ],

    /**
     * Callback to run when the screen is dismissed.
     * @type {function(string)}
     */
    callback_: null,

    /** @override */
    decorate: function() {
      $('confirm-password-input').addEventListener(
          'keydown', this.onPasswordFieldKeyDown_.bind(this));
    },

    /**
     * Screen controls in bottom strip.
     * @type {Array.<HTMLButtonElement>} Buttons to be put in the bottom strip.
     */
    get buttons() {
      var buttons = [];

      var confirmButton = this.ownerDocument.createElement('button');
      confirmButton.textContent =
          loadTimeData.getString('confirmPasswordConfirmButton');
      confirmButton.addEventListener('click',
                                     this.onConfirmPassword_.bind(this));
      buttons.push(confirmButton);

      return buttons;
    },

    get defaultControl() {
      return $('confirm-password-input');
    },

    /**
     * Handle 'keydown' event on password input field.
     */
    onPasswordFieldKeyDown_: function(e) {
      if (e.keyIdentifier == 'Enter')
        this.onConfirmPassword_();
    },

    /**
     * Invoked when user clicks on the 'confirm' button.
     */
    onConfirmPassword_: function() {
      this.callback_($('confirm-password-input').value);
    },

    /**
     * Shows the confirm password screen.
     * @param {function(string)} callback The callback to be invoked when the
     *     screen is dismissed.
     */
    show: function(callback) {
      this.callback_ = callback;

      $('confirm-password-input').value = '';
      Oobe.showScreen({id: SCREEN_CONFIRM_PASSWORD});
      $('progress-dots').hidden = true;
    }
  };
});
