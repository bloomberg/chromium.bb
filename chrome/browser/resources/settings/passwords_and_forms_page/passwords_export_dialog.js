// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-export-dialog' is the dialog that allows exporting
 * passwords.
 */

(function() {
'use strict';

Polymer({
  is: 'passwords-export-dialog',

  /**
   * The interface for callbacks to the browser.
   * Defined in passwords_section.js
   * @type {PasswordManager}
   * @private
   */
  passwordManager_: null,

  /** @override */
  attached: function() {
    this.$.dialog.showModal();

    this.passwordManager_ = PasswordManagerImpl.getInstance();
  },

  /** Closes the dialog. */
  close: function() {
    this.$.dialog.close();
  },

  /**
   * Fires an event that should trigger the password export process.
   * @private
   */
  onExportTap_: function() {
    this.passwordManager_.exportPasswords();
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancelButtonTap_: function() {
    this.close();
  },
});
})();