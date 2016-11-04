// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying AD domain joining and AD
 * Authenticate user screens.
 */

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
    adWelcomeMessage: String
  },

  /** @private */
  realmChanged_: function() {
    this.adWelcomeMessage =
        loadTimeData.getStringF('AdAuthWelcomeMessage', this.realm);
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
    this.$.userInput.value = user || '';
    this.$.machineNameInput.value = machineName || '';
    this.$.passwordInput.value = '';
    this.$.passwordInput.isInvalid = !!user;
    this.focus();
  },

  /** @private */
  onSubmit_: function() {
    if (this.showMachineInput && !this.$.machineNameInput.checkValidity())
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
