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
     * Event handler that is invoked just before the screen in shown.
     */
    onBeforeShow: function() {
      $('progress-dots').hidden = true;
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
