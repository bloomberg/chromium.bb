// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'error-dialog' is a popup dialog that displays error messages
 * in the user manager.
 */
(function() {
Polymer({
  is: 'error-dialog',

  properties: {
    /**
     * True if the element is currently hidden.
     * @private {boolean}
     */
    popupHidden_: {
      type: Boolean,
      value: true
    },

    /**
     * The message shown in the dialog.
     * @private {string}
     */
    message_: {
      type: String,
      value: ''
    }
  },

  /**
   * Displays the popup populated with the given message.
   * @param {string} message Error message to show.
   */
  show: function(message) {
    this.message_ = message;
    this.popupHidden_ = false;

    this.async(function() {
      this.$$('paper-icon-button').focus();
    }.bind(this));
  },

  /**
   * Hides the popup.
   * @private
   */
  onCloseTap_: function() {
    this.popupHidden_ = true;
  }
});
})();
