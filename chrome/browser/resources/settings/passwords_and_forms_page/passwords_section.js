// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.PasswordUiEntry}}} */
var PasswordUiEntryEvent;

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.ExceptionPair}}} */
var ExceptionPairEntryEvent;

(function() {
'use strict';

Polymer({
  is: 'passwords-section',

  properties: {
    /**
     * An array of passwords to display.
     * @type {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
     */
    savedPasswords: {
      type: Array,
      value: function() { return []; },
    },

    /**
     * Whether passwords can be shown or not.
     * @type {boolean}
     */
    showPasswords: {
      type: Boolean,
    },

    /**
     * An array of sites to display.
     * @type {!Array<!chrome.passwordsPrivate.ExceptionPair>}
     */
    passwordExceptions: {
      type: Array,
      value: function() { return []; },
    },


    /**
     * Assigning a non-null value triggers the add/edit dialog.
     * @private {?chrome.passwordsPrivate.PasswordUiEntry}
     */
    activePassword: Object,

    /** Filter on the saved passwords and exceptions. */
    filter: {
      type: String,
      value: '',
    },
  },

  listeners: {
    'passwordList.scroll': 'closeMenu_',
  },

  /**
   * Sets the password in the current password dialog if the loginPair matches.
   * @param {!chrome.passwordsPrivate.LoginPair} loginPair
   * @param {string} password
   */
  setPassword: function(loginPair, password) {
    if (this.activePassword &&
        this.activePassword.loginPair.originUrl == loginPair.originUrl &&
        this.activePassword.loginPair.username == loginPair.username) {
      this.$$('password-edit-dialog').password = password;
    }
  },

  /**
   * Shows the edit password dialog.
   * @private
   */
  onMenuEditPasswordTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.menu);
    this.activePassword =
        /** @type {chrome.passwordsPrivate.PasswordUiEntry} */(menu.itemData);
    menu.closeMenu();
  },

  /** @private */
  unstampPasswordEditDialog_: function(e) {
    this.activePassword = null;
  },

  /**
   * @param {!Array<!chrome.passwordsPrivate.PasswordUiEntry>} savedPasswords
   * @param {string} filter
   * @return {!Array<!chrome.passwordsPrivate.PasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_: function(savedPasswords, filter) {
    if (!filter)
      return savedPasswords;

    return savedPasswords.filter(function(password) {
      return password.loginPair.originUrl.includes(filter) ||
             password.loginPair.username.includes(filter);
    });
  },

  /**
   * @param {string} filter
   * @return {function(!chrome.passwordsPrivate.ExceptionPair): boolean}
   * @private
   */
  passwordExceptionFilter_: function(filter) {
    return function(exception) {
      return exception.exceptionUrl.includes(filter);
    };
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.menu);
    var data =
        /** @type {chrome.passwordsPrivate.PasswordUiEntry} */(menu.itemData);
    this.fire('remove-saved-password', data.loginPair);
    menu.closeMenu();
  },

  /**
   * Fires an event that should delete the password exception.
   * @param {!ExceptionPairEntryEvent} e The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_: function(e) {
    this.fire('remove-password-exception', e.model.item.exceptionUrl);
  },

  /**
   * Creates an empty password of specified length.
   * @param {number} length
   * @return {string} password
   * @private
   */
  getEmptyPassword_: function(length) { return ' '.repeat(length); },

  /**
   * Toggles the overflow menu.
   * @param {!Event} e The polymer event.
   * @private
   */
  onPasswordMenuTap_: function(e) {
    var menu = /** @type {CrSharedMenuElement} */(this.$.menu);
    var target = /** @type {!Element} */(Polymer.dom(e).localTarget);
    var passwordUiEntryEvent = /** @type {!PasswordUiEntryEvent} */(e);

    menu.toggleMenu(target, passwordUiEntryEvent.model.item);
    e.stopPropagation();  // Prevent the tap event from closing the menu.
  },

  /**
   * Closes the overflow menu.
   * @private
   */
  closeMenu_: function() {
    /** @type {CrSharedMenuElement} */(this.$.menu).closeMenu();
  },
});
})();
