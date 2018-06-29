// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Demo Setup
 * screen.
 */

Polymer({
  is: 'demo-setup-md',

  properties: {
    /**
     * Whether offline demo setup was selected. Available setup types: online
     * and offline.
     */
    is_offline_setup_: {
      type: Boolean,
      value: false,
    },

    /** Ordered array of screen ids that are a part of demo setup flow. */
    screens_: {
      type: Array,
      readonly: true,
      value: function() {
        return [
          'demoSetupSettingsDialog', 'demoSetupProgressDialog',
          'demoSetupErrorDialog'
        ];
      },
    },
  },

  /** Resets demo setup flow to the initial screen. */
  reset: function() {
    this.showScreen_(this.screens_[0]);
  },

  /**
   * Called when demo mode setup finished.
   * @param {string} isSuccess Whether demo setup finished successfully.
   * @param {boolean} message Error message to be displayed to the user if setup
   *  finished with an error.
   */
  onSetupFinished: function(isSuccess, message) {
    if (!isSuccess) {
      this.showScreen_('demoSetupErrorDialog');
    }
  },

  /**
   * Shows screen with the given id. Method exposed for testing environment.
   * @param {string} id Screen id.
   */
  showScreenForTesting: function(id) {
    this.showScreen_(id);
  },

  /**
   * Shows progress dialog and starts demo setup.
   * @private
   */
  startSetup_: function() {
    this.showScreen_('demoSetupProgressDialog');
    if (this.is_offline_setup_) {
      chrome.send('login.DemoSetupScreen.userActed', ['offline-setup']);
    } else {
      chrome.send('login.DemoSetupScreen.userActed', ['online-setup']);
    }
  },

  /**
   * Shows screen with the given id.
   * @param {string} id Screen id.
   * @private
   */
  showScreen_: function(id) {
    this.hideScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideScreens_: function() {
    for (let id of this.screens_) {
      var screen = this.$[id];
      assert(screen);
      screen.hidden = true;
    }
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_: function() {
    const selected = this.$.setupGroup.selected;
    this.is_offline_setup_ = (selected == 'offlineSetup');
    this.startSetup_();
  },

  /**
   * Retry button click handler.
   * @private
   */
  onRetryClicked_: function() {
    this.startSetup_();
  },

  /**
   * Close button click handler.
   * @private
   */
  onCloseClicked_: function() {
    chrome.send('login.DemoSetupScreen.userActed', ['close-setup']);
  },
});
