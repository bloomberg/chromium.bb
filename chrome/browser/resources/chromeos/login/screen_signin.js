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
     * Helper function to toggle throubber and input fields.
     * @param {boolean} enable True to enable credential input UI.
     * @private
     */
    enableInputs_: function(enable) {
      $('login-throbber').hidden = enable;
      $('email').hidden = !enable;
      $('password').hidden = !enable;
      $('signin-button').hidden = !enable;
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
     * Handles sign in button click.
     * @private
     */
    handleSigninClick_: function(e) {
      this.state = SigninScreen.STATE_AUTHENTICATING;

      chrome.send('authenticateUser',
          [$('email').value, $('password').value]);

      // TODO(xiyuan): Hook up with LoginDisplay::ShowError to show and reset
      // crendentical input UI.
    }
  };

  return {
    SigninScreen: SigninScreen
  };
});
