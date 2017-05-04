// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordListItem represents one row in the list of passwords.
 * It needs to be its own component because FocusRowBehavior provides good a11y.
 */

Polymer({
  is: 'password-list-item',

  behaviors: [FocusRowBehavior],

  properties: {
    /**
     * The password whose info should be displayed.
     * @type {!chrome.passwordsPrivate.PasswordUiEntry}
     */
    item: Array,
  },

  /**
   * Creates an empty password of specified length.
   * @param {number} length
   * @return {string} password
   * @private
   */
  getEmptyPassword_: function(length) {
    return ' '.repeat(length);
  },

  /**
   * Opens the password action menu.
   * @private
   */
  onPasswordMenuTap_: function() {
    this.fire(
        'password-menu-tap', {target: this.$.passwordMenu, item: this.item});
  },
});
