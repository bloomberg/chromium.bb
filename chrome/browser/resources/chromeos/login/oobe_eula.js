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
     * "Accepot and continue" button is disabled until content is loaded.
     */
    acceptButtonDisabled: {
      type: Boolean,
      value: true,
    },

    /**
     * If "Report anonymous usage stats" checkbox is checked.
     */
    usageStatsChecked: {
      type: Boolean,
      value: false,
    },

    /**
     * Reference to OOBE screen object.
     * @type {!OobeTypes.Screen}
     */
    screen: {
      type: Object,
    },
  },

  focus: function() {
    if (this.eulaLoadingScreenShown) {
      this.$.eulaLoadingDialog.show();
    } else {
      this.$.eulaDialog.show();
    }
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

  /**
   * On-change event handler for usageStats.
   *
   * @private
   */
  onUsageChanged_: function() {
    this.screen.onUsageStatsClicked_(this.$.usageStats.checked);
  },

  /**
   * On-tap event handler for installationSettings.
   *
   * @private
   */
  onInstallationSettingsClicked_: function() {
    chrome.send('eulaOnInstallationSettingsPopupOpened');
    $('popup-overlay').hidden = false;
    $('installation-settings-ok-button').focus();
  },

  /**
   * On-tap event handler for stats-help-link.
   *
   * @private
   */
  onUsageStatsHelpLinkClicked_: function() {
    chrome.send('eulaOnLearnMore');
  },

  /**
   * On-tap event handler for back button.
   *
   * @private
   */
  onEulaBackButtonPressed_: function() {
    chrome.send('login.EulaScreen.userActed', ['back-button']);
  },
});
