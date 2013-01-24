// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Create managed user implementation.
 */

cr.define('login', function() {
  /**
   * Creates a new managed user creation screen div.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var ManagedUserCreationScreen = cr.ui.define('div');

  /**
   * Registers with Oobe.
   */
  ManagedUserCreationScreen.register = function() {
    var screen = $('managed-user-creation');
    ManagedUserCreationScreen.decorate(screen);
    Oobe.getInstance().registerScreen(screen);
  };

  ManagedUserCreationScreen.prototype = {
    __proto__: HTMLDivElement.prototype,

    lastVerifiedName_: null,
    lastIncorrectUserName_: null,

    /** @override */
    decorate: function() {
      var closeButton = $('managed-user-creation-cancel-button');
      var userNameField = $('managed-user-creation-name');
      var passwordField = $('managed-user-creation-password');
      var password2Field = $('managed-user-creation-password-confirm');

      closeButton.addEventListener('click', function(e) {
        Oobe.goBack();
      });
      var creationScreen = this;
      userNameField.addEventListener('keydown', function(e) {
        if (e.keyIdentifier == 'Enter') {
          if (userNameField.value.length > 0)
            passwordField.focus();
          e.stopPropagation();
          return;
        }
        creationScreen.clearUserNameError_();
      });

      userNameField.addEventListener('keyup', function(e) {
        creationScreen.checkUserName_();
      });

      passwordField.addEventListener('keydown', function(e) {
        creationScreen.passwordErrorVisible = false;
        if (e.keyIdentifier == 'Enter') {
          if (passwordField.value.length > 0) {
            password2Field.focus();
            creationScreen.updateContinueButton_();
          }
          e.stopPropagation();
        }
      });
      password2Field.addEventListener('keydown', function(e) {
        creationScreen.passwordErrorVisible = false;
        if (e.keyIdentifier == 'Enter') {
          if (creationScreen.updateContinueButton_())
            creationScreen.validateAndContinue_();
          e.stopPropagation();
        }
      });

      password2Field.addEventListener('keyup', function(e) {
        creationScreen.updateContinueButton_();
      });
      passwordField.addEventListener('keyup', function(e) {
        creationScreen.updateContinueButton_();
      });
    },

    /**
     * Calls backend part to check if current user name is valid/not taken.
     * Results in call to either managedUserNameOk or managedUserNameError.
     * @private
     */
    checkUserName_: function() {
      var userName = $('managed-user-creation-name').value;

      // Avoid flickering
      if (userName == this.lastIncorrectUserName_ ||
          userName == this.lastVerifiedName_) {
        return;
      }
      if (userName.length > 0) {
        chrome.send('checkLocallyManagedUserName', [userName]);
      } else {
        this.nameErrorVisible = false;
        this.lastVerifiedName_ = null;
        this.lastIncorrectUserName_ = null;
      }
    },

    /**
     * Called by backend part in case of successful name validation.
     * @param {string} name - name that was validated.
     */
    managedUserNameOk: function(name) {
      this.lastVerifiedName_ = name;
      this.lastIncorrectUserName_ = null;
      if ($('managed-user-creation-name').value == name)
        this.clearUserNameError_();
      this.updateContinueButton_();
    },

    /**
     * Called by backend part in case of name validation failure.
     * @param {string} name - name that was validated.
     * @param {string} errorText - reason why this name is invalid.
     */
    managedUserNameError: function(name, errorText) {
      this.lastIncorrectUserName_ = name;
      this.lastVerifiedName_ = null;

      var userNameField = $('managed-user-creation-name');
      if (userNameField.value == this.lastIncorrectUserName_) {
        this.nameErrorVisible = true;
        $('managed-user-creation-name-error').textContent = errorText;
        $('managed-user-creation-continue-button').disabled = true;
      }
    },

    /**
     * Clears user name error, if name is no more guaranteed to be invalid.
     * @private
     */
    clearUserNameError_: function() {
      // Avoid flickering
      if ($('managed-user-creation-name').value == this.lastIncorrectUserName_)
        return;
      this.nameErrorVisible = false;
    },

    /**
     * Called by backend part in case of password validation failure.
     * @param {string} errorText - reason why this password is invalid.
     */
    showPasswordError: function(errorText) {
      $('managed-user-creation-password-error').textContent = errorText;
      this.passwordErrorVisible = true;
      $('managed-user-creation-password').focus();
      $('managed-user-creation-continue-button').disabled = true;
    },

    /**
     * True if user name error should be displayed.
     * @type {boolean}
     */
    set nameErrorVisible(value) {
      $('managed-user-creation-name-error').
          classList[value ? 'add' : 'remove']('error');
      $('managed-user-creation-name').
          classList[value ? 'add' : 'remove']('duplicate-name');
      if (!value)
        $('managed-user-creation-name-error').textContent = '';
    },

    /**
     * True if user name error should be displayed.
     * @type {boolean}
     */
    set passwordErrorVisible(value) {
      $('managed-user-creation-password-error').
          classList[value ? 'add' : 'remove']('error');
      if (!value)
        $('managed-user-creation-password-error').textContent = '';
    },

    /**
     * Updates state of Continue button after minimal checks.
     * @return {boolean} true, if form seems to be valid.
     * @private
     */
    updateContinueButton_: function() {
      var firstPassword = $('managed-user-creation-password').value;
      var secondPassword = $('managed-user-creation-password-confirm').value;
      var userName = $('managed-user-creation-name').value;

      var canProceed =
          (firstPassword.length > 0) &&
          (firstPassword.length == secondPassword.length) &&
           this.lastVerifiedName_ &&
           (userName == this.lastVerifiedName_);

      $('managed-user-creation-continue-button').disabled = !canProceed;
      return canProceed;
    },

    /**
     * Does sanity check and calls backend with current user name/password pair
     * to create a user. May result in showPasswordError.
     * @private
     */
    validateAndContinue_: function() {
      var firstPassword = $('managed-user-creation-password').value;
      var secondPassword = $('managed-user-creation-password-confirm').value;
      var userName = $('managed-user-creation-name').value;

      if (firstPassword != secondPassword) {
        this.showPasswordError(
            localStrings.getString('createManagedUserPasswordMismatchError'));
        return;
      }
      chrome.send('tryCreateLocallyManagedUser', [userName, firstPassword]);
    },

    /**
     * Screen controls.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var continueButton = this.ownerDocument.createElement('button');
      continueButton.id = 'managed-user-creation-continue-button';
      continueButton.textContent =
          localStrings.getString('createManagedUserContinueButton');
      buttons.push(continueButton);

      var creationScreen = this;
      continueButton.addEventListener('click', function(e) {
        creationScreen.validateAndContinue_();
        e.stopPropagation();
      });

      return buttons;
    },

    /**
     * Update state of login header so that necessary buttons are displayed.
     */
    onBeforeShow: function(data) {
      $('login-header-bar').signinUIState =
          SIGNIN_UI_STATE.MANAGED_USER_CREATION;
    },

    /**
     * Update state of login header so that necessary buttons are displayed.
     */
    onBeforeHide: function() {
      $('login-header-bar').signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('managed-user-creation-name');
    },

    /**
     * True if the the screen is disabled (handles no user interaction).
     * @type {boolean}
     */
    disabled_: false,

    get disabled() {
      return this.disabled_;
    },

    set disabled(value) {
      this.disabled_ = value;
      var controls = this.querySelectorAll('button,input');
      for (var i = 0, control; control = controls[i]; ++i) {
        control.disabled = value;
      }
      $('login-header-bar').disabled = value;
    },

  };

  /**
   * Show Create managed user screen.
   */
  ManagedUserCreationScreen.show = function() {
    Oobe.getInstance().headerHidden = false;
    var screen = $('managed-user-creation');

    Oobe.showScreen({id: SCREEN_CREATE_MANAGED_USER});

    // Clear all fields.
    $('managed-user-creation-password').value = '';
    $('managed-user-creation-password-confirm').value = '';
    $('managed-user-creation-name').value = '';
    screen.lastVerifiedName_ = null;
    screen.lastIncorrectUserName_ = null;
    screen.passwordErrorVisible = false;
    screen.nameErrorVisible = false;

    $('managed-user-creation-continue-button').disabled = true;
  };

  ManagedUserCreationScreen.managedUserNameOk = function(name) {
    var screen = $('managed-user-creation');
    screen.managedUserNameOk(name);
  };

  ManagedUserCreationScreen.managedUserNameError = function(name, error) {
    var screen = $('managed-user-creation');
    screen.managedUserNameError(name, error);
  };

  ManagedUserCreationScreen.showPasswordError = function(error) {
    var screen = $('managed-user-creation');
    screen.showPasswordError(error);
  };

  return {
    ManagedUserCreationScreen: ManagedUserCreationScreen
  };
});
