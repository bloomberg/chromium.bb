// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-edit-dialog' is the dialog that allows showing a
 *     saved password.
 */

(function() {
'use strict';

Polymer({
  is: 'password-edit-dialog',

  properties: {
    /**
     * The password that is being displayed.
     * @type {!chrome.passwordsPrivate.PasswordUiEntry}
     */
    item: {
      type: Object,
      value: null,
    },
  },

  /** Opens the dialog. */
  open: function() {
    this.$.dialog.open();
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
