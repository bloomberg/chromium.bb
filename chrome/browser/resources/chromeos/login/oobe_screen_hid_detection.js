// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe HID detection screen implementation.
 */

login.createScreen('HIDDetectionScreen', 'hid-detection', function() {
  return {
    /**
     * Button to move to usual OOBE flow after detection.
     * @private
     */
    continueButton_: null,

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];
      var continueButton = this.ownerDocument.createElement('button');
      continueButton.id = 'hid-continue-button';
      continueButton.textContent = loadTimeData.getString(
          'hidDetectionContinue');
      continueButton.addEventListener('click', function(e) {
        chrome.send('HIDDetectionOnContinue');
        e.stopPropagation();
      });
      buttons.push(continueButton);
      this.continueButton_ = continueButton;

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return this.continueButton_;
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      $('hid-continue-button').disabled = true;
    },
  };
});
