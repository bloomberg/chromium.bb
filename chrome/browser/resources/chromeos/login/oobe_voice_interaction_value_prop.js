// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design voice
 * interaction value prop screen.
 */

Polymer({
  is: 'voice-interaction-value-prop-md',

  /**
   * Returns element by its id.
   */
  getElement: function(id) {
    return this.$[id];
  },

  /**
   * On-tap event handler for no thanks button.
   *
   * @private
   */
  onNoThanksTap_: function() {
    chrome.send(
        'login.VoiceInteractionValuePropScreen.userActed',
        ['no-thanks-pressed']);
  },

  /**
   * On-tap event handler for continue button.
   *
   * @private
   */
  onContinueTap_: function() {
    chrome.send(
        'login.VoiceInteractionValuePropScreen.userActed',
        ['continue-pressed']);
  },
});
