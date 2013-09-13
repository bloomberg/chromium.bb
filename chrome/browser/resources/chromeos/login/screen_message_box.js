// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple message box screen implementation.
 */

login.createScreen('MessageBoxScreen', 'message-box', function() { return {
    EXTERNAL_API: [
      'show'
    ],

    /**
     * Callback to run when the screen is dismissed.
     * @type {function()}
     */
    callback_: null,

    /**
     * Ok button of the message box.
     * @type {HTMLButtonElement}
     */
    okButton_: null,

    /**
     * Saved hidden status of 'progress-dots'.
     * @type {boolean}
     */
    savedProgressDotsHidden_: null,

    /**
     * Screen controls in bottom strip.
     * @type {Array.<HTMLButtonElement>} Buttons to be put in the bottom strip.
     */
    get buttons() {
      var buttons = [];

      this.okButton_ = this.ownerDocument.createElement('button');
      this.okButton_.addEventListener('click', this.onDismiss_.bind(this));
      buttons.push(this.okButton_);

      return buttons;
    },

    get defaultControl() {
      return this.okButton_;
    },

    /**
     * Invoked when user clicks on the ok button.
     */
    onDismiss_: function() {
      this.callback_();
      $('progress-dots').hidden = this.savedProgressDotsHidden_;
    },

    /**
     * Shows the no password warning screen.
     * @param {string} title Title string of the message box.
     * @param {string} message Body text of the message box.
     * @param {string} okLabel Label text for the okay button.
     * @param {function()} callback The callback to be invoked when the
     *     screen is dismissed.
     */
    show: function(title, message, okLabel, callback) {
      $('message-box-title').textContent = title;
      $('message-box-body').textContent = message;
      this.okButton_.textContent = okLabel;
      this.callback_ = callback;

      Oobe.showScreen({id: SCREEN_MESSAGE_BOX});

      this.savedProgressDotsHidden_ = $('progress-dots').hidden;
      $('progress-dots').hidden = true;
    }
  };
});
