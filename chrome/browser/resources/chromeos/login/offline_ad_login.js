// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying AD domain joining and AD
 * Authenticate user screens.
 */
// Possible error states of the screen. Must be in the same order as
// ActiveDirectoryErrorState enum values.
/** @enum {number} */ var ACTIVE_DIRECTORY_ERROR_STATE = {
  NONE: 0,
  MACHINE_NAME_INVALID: 1,
  MACHINE_NAME_TOO_LONG: 2,
  BAD_USERNAME: 3,
  BAD_PASSWORD: 4,
};

Polymer({
  is: 'offline-ad-login',

  properties: {
    /**
     * Whether the UI disabled.
     */
    disabled: {
      type: Boolean,
      value: false,
      observer: 'disabledChanged_'
    },
    /**
     * Whether to show machine name input field.
     */
    showMachineInput: {
      type: Boolean,
      value: false
    },
    /**
     * The kerberos realm (AD Domain), the machine is part of.
     */
    realm: {
      type: String,
      observer: 'realmChanged_'
    },
    /**
     * The user kerberos default realm. Used for autocompletion.
     */
    userRealm: String,
    /**
     * Welcome message on top of the UI.
     */
    adWelcomeMessage: String,
    /**
     * Error message for the machine name input.
     */
    machineNameError: String,
  },

  /** @private */
  realmChanged_: function() {
    this.adWelcomeMessage =
        loadTimeData.getStringF('adAuthWelcomeMessage', this.realm);
  },

  /** @private */
  disabledChanged_: function() {
    this.$.gaiaCard.classList.toggle('disabled', this.disabled);
  },

  focus: function() {
    if (this.showMachineInput && /** @type {string} */ (
            this.$.machineNameInput.value) == '') {
      this.$.machineNameInput.focus();
    } else if (/** @type {string} */ (this.$.userInput.value) == '') {
      this.$.userInput.focus();
    } else {
      this.$.passwordInput.focus();
    }
  },

  /**
   * @param {string|undefined} user
   * @param {string|undefined} machineName
   */
  setUser: function(user, machineName) {
    if (this.userRealm && user)
      user = user.replace(this.userRealm, '');
    this.$.userInput.value = user || '';
    this.$.machineNameInput.value = machineName || '';
    this.$.passwordInput.value = '';
    this.focus();
  },

  /**
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} error_state
   */
  setInvalid: function(error_state) {
    this.$.machineNameInput.isInvalid = false;
    this.$.userInput.isInvalid = false;
    this.$.passwordInput.isInvalid = false;
    switch (error_state) {
      case ACTIVE_DIRECTORY_ERROR_STATE.NONE:
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID:
        this.machineNameError =
            loadTimeData.getString('adJoinErrorMachineNameInvalid');
        this.$.machineNameInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG:
        this.machineNameError =
            loadTimeData.getString('adJoinErrorMachineNameTooLong');
        this.$.machineNameInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.BAD_USERNAME:
        this.$.userInput.isInvalid = true;
        break;
      case ACTIVE_DIRECTORY_ERROR_STATE.BAD_PASSWORD:
        this.$.passwordInput.isInvalid = true;
        break;
    }
  },

  /** @private */
  onSubmit_: function() {
    if (this.showMachineInput && !this.$.machineNameInput.checkValidity())
      return;
    if (!this.$.userInput.checkValidity())
      return;
    if (!this.$.passwordInput.checkValidity())
      return;
    var user = /** @type {string} */ (this.$.userInput.value);
    if (!user.includes('@') && this.userRealm)
      user += this.userRealm;
    var msg = {
      'machinename': this.$.machineNameInput.value,
      'username': user,
      'password': this.$.passwordInput.value
    };
    this.$.passwordInput.value = '';
    this.fire('authCompleted', msg);
  },
});
