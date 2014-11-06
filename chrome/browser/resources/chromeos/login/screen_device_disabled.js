// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Device disabled screen implementation.
 */

login.createScreen('DeviceDisabledScreen', 'device-disabled', function() {
  return {
    EXTERNAL_API: [
      'setMessage'
    ],

    /**
     * The visibility of the cancel button in the header bar is controlled by a
     * global. Although the device disabling screen hides the button, a
     * notification intended for an earlier screen (e.g animation finished)
     * could re-show the button. If this happens, the current screen's cancel()
     * method will be shown when the user actually clicks the button. Make sure
     * that this is a no-op.
     */
    cancel: function() {
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     */
    onBeforeShow: function() {
      $('progress-dots').hidden = true;
      var headerBar = $('login-header-bar');
      headerBar.allowCancel = false;
      headerBar.showGuestButton = false;
      headerBar.signinUIState = SIGNIN_UI_STATE.HIDDEN;
    },

    /**
      * Sets the message to show to the user.
      * @param {string} message The message to show to the user.
      * @private
      */
    setMessage: function(message) {
      $('device-disabled-message').textContent = message;
    }
  };
});
