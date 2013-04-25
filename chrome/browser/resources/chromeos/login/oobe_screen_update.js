// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe update screen implementation.
 */

login.createScreen('UpdateScreen', 'update', function() {
  return {
    EXTERNAL_API: [
      'enableUpdateCancel'
    ],

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('updateScreenTitle');
    },

    /**
     * Cancels the screen.
     */
    cancel: function() {
      // It's safe to act on the accelerator even if it's disabled on official
      // builds, since Chrome will just ignore the message in that case.
      var updateCancelHint = $('update-cancel-hint').firstElementChild;
      updateCancelHint.textContent =
          loadTimeData.getString('cancelledUpdateMessage');
      chrome.send('cancelUpdate');
    },

    /**
     * Makes 'press Escape to cancel update' hint visible.
     */
    enableUpdateCancel: function() {
      $('update-cancel-hint').hidden = false;
    }
  };
});

