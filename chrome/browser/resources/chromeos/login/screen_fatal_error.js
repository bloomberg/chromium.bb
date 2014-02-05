// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple message box screen implementation.
 */

login.createScreen('FatalErrorScreen', 'fatal-error', function() { return {
    EXTERNAL_API: [
      'show'
    ],

    /**
     * Callback to run when the screen is dismissed.
     * @type {function()}
     */
    callback_: null,

    /**
     * Saved hidden status of 'progress-dots'.
     * @type {boolean}
     */
    savedProgressDotsHidden_: null,

    /** @override */
    decorate: function() {
      $('fatal-error-dismiss-button').addEventListener(
          'click', this.onDismiss_.bind(this));
    },

    get defaultControl() {
      return $('fatal-error-dismiss-button');
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
     * @param {function()} callback The callback to be invoked when the
     *     screen is dismissed.
     */
    show: function(callback) {
      Oobe.getInstance().headerHidden = true;
      this.callback_ = callback;

      Oobe.showScreen({id: SCREEN_FATAL_ERROR});

      this.savedProgressDotsHidden_ = $('progress-dots').hidden;
      $('progress-dots').hidden = true;
    }
  };
});
