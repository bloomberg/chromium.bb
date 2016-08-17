// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Terms Of Service
 * screen.
 */

Polymer({
  is: 'oobe-eula-md',

  properties: {
    /**
     * Shows "Loading..." section.
     */
    eulaLoadingScreenShown: {
      type: Boolean,
      value: false,
    },

    /**
     * Shows Terms Of Service frame.
     */
    eulaScreenShown: {
      type: Array,
    },

    /**
     * "Accepot and continue" button is disabled until content is loaded.
     */
    acceptButtonDisabled: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Event handler that is invoked when 'chrome://terms' is loaded.
   */
  onFrameLoad_: function() {
    this.acceptButtonDisabled = false;
  },

  /**
   * This is called when strings are updated.
   */
  updateLocalizedContent: function(event) {
    // This forces frame to reload.
    this.$.crosEulaFrame.src = this.$.crosEulaFrame.src;
  },

  /**
   * This is 'on-tap' event handler for 'Accept' button.
   */
  eulaAccepted_: function(event) {
    chrome.send('login.EulaScreen.userActed', ['accept-button']);
  },
});
