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
     * @type {!Array<!string>}
     */
    passwordExceptions: {
      type: Array,
      value: function() { return []; },
    },
  },

  listeners: {
    'passwordList.scroll': 'closeMenu_',
    'tap': 'closeMenu_',
  },

  /**
   * Fires an event that should delete the saved password.
   * @private
   */
  onMenuRemovePasswordTap_: function() {
    var menu = /** @type {CrSharedMenuElement} */(this.$.menu);
    this.fire('remove-saved-password', menu.itemData);
    menu.closeMenu();
  },

  /**
   * Fires an event that should delete the password exception.
   * @param {!{model: !{item: !string}}} e The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_: function(e) {
    this.fire('remove-password-exception', e.model.item);
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

    menu.toggleMenu(target, passwordUiEntryEvent.model.item.loginPair);
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
