// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 *
 * @group Chrome Settings Elements
 * @element passwords-section
 */
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
     * An array of sites to display.
     * @type {!Array<!string>}
     */
    passwordExceptions: {
      type: Array,
      value: function() { return []; },
    },
  },

  /**
   * Fires an event that should delete the passwordException.
   * @param {!{model: !{item: !string}}} e The polymer event.
   * @private
   */
  onRemovePasswordException_: function(e) {
    this.fire('remove-password-exception', e.model.item);
  },

  /**
   * Creates an empty password of specified length.
   * @param {number} length
   * @return {string} password
   * @private
   */
  getEmptyPassword_: function(length) { return ' '.repeat(length); },
});
})();
