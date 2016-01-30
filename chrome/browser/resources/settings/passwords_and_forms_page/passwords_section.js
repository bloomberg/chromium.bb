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
  },

  /**
   * Creates an empty password of specified length.
   * @param {number} length
   * @return {string} password
   * @private
   */
  getEmptyPassword_(length) { return ' '.repeat(length); },
});
})();
