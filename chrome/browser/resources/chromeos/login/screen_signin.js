// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe signin screen implementation.
 */

cr.define('login', function() {
  /**
   * Creates a new sign in screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var SigninScreen = cr.ui.define('div');

  // Constants for sign in screen states
  SigninScreen.STATE_INPUT = 0;
  SigninScreen.STATE_AUTHENTICATING = 1;

  /**
   * Registers with Oobe.
   */
  SigninScreen.register = function() {
    var screen = $('signin');
    SigninScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  SigninScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      $('email').addEventListener('keydown', this.handleKeyDown_.bind(this));
      $('email').addEventListener('blur', this.handleEmailBlur_);
      $('password').addEventListener('keydown', this.handleKeyDown_.bind(this));
      $('signin-button-in-place').addEventListener('click',
          this.handleSigninClick_.bind(this));
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return localStrings.getString('signinScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var signinButton = this.ownerDocument.createElement('button');
      signinButton.id = 'signin-button';
      signinButton.textContent = localStrings.getString('signinButton');
      signinButton.addEventListener('click',
          this.handleSigninClick_.bind(this));
      buttons.push(signinButton);

      return buttons;
    },

    /**
     * Helper function to toggle throbber and input fields.
     * @param {boolean} enable True to enable credential input UI.
     * @private
     */
    enableInputs_: function(enable) {
      $('login-throbber').hidden = enable;
      $('email').hidden = !enable;
      $('password').hidden = !enable;
      $('signin-button').hidden = !enable;
      $('signin-button-in-place').hidden = !enable;
    },

    /**
     * Sets UI state.
     */
    set state(newState) {
      if (newState == SigninScreen.STATE_INPUT) {
        this.enableInputs_(true);
      } else if (newState == SigninScreen.STATE_AUTHENTICATING) {
        this.enableInputs_(false);
     }
    },

    /**
     * Clears input fields and switches to input mode.
     */
    reset: function() {
      $('email').value = '';
      $('password').value = '';
      $('email').focus();
      this.state = SigninScreen.STATE_INPUT;
    },

    /**
     * Validates input fields.
     */
    checkInput: function() {
      // Validate input.
      if (!$('email').value) {
         $('email').focus();
        return false;
      }

      if (!$('password').value) {
        $('password').focus();
        return false;
      }

      return true;
    },

    /**
     * Handles sign in button click.
     * @private
     */
    handleSigninClick_: function(e) {
      if (!this.checkInput())
        return;

      this.state = SigninScreen.STATE_AUTHENTICATING;

      chrome.send('authenticateUser',
          [$('email').value, $('password').value]);
    },

    /**
     * Handles keyboard event.
     * @private
     */
    handleKeyDown_: function(e) {
      // Handle 'Enter' key for 'email' and 'password' field.
      if (e.keyIdentifier == 'Enter') {
        if (e.target.id == 'email') {
          if (e.target.value)
            $('password').focus();
        } else if (e.target.id == 'password') {
          this.handleSigninClick_();
        }
      }
    },

    /**
     * Handles blur event of email field.
     * @private
     */
    handleEmailBlur_: function(e) {
      var emailElement = $('email');
      if (emailElement.value.indexOf('@') == -1)
        emailElement.value += '@gmail.com';
    }
  };

  /**
   * Expose 'reset' method that resets sign-in screen.
   * @public
   */
  SigninScreen.reset = function() {
    $('signin').reset();
  };

  return {
    SigninScreen: SigninScreen
  };
});
