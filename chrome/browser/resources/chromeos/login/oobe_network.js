// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying network selection OOBE dialog.
 */

Polymer({
  is: 'oobe-network-md',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  observers:
      ['onDemoModeSetupChanged_(isDemoModeSetup, offlineDemoModeEnabled)'],

  properties: {
    /**
     * Whether network dialog is shown as a part of demo mode setup flow.
     * Additional custom elements can be displayed on network list in demo mode
     * setup.
     */
    isDemoModeSetup: false,

    /**
     * Whether offline demo mode is enabled. If it is enabled offline setup
     * option will be shown in UI.
     */
    offlineDemoModeEnabled: false,

    /**
     * Whether device is connected to the network.
     * @private
     */
    isConnected_: false,
  },

  /** Called when dialog is shown. */
  onBeforeShow: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeShow)
        behavior.onBeforeShow.call(this);
    });
    this.$.networkSelectLogin.onBeforeShow();
  },

  /** Called when dialog is hidden. */
  onBeforeHide: function() {
    this.behaviors.forEach((behavior) => {
      if (behavior.onBeforeHide)
        behavior.onBeforeHide.call(this);
    });
    this.$.networkSelectLogin.onBeforeHide();
  },

  /** @override */
  ready: function() {
    this.updateLocalizedContent();
  },

  /** Shows the dialog. */
  show: function() {
    this.$.networkDialog.show();
  },

  /** Updates localized elements of the UI. */
  updateLocalizedContent: function() {
    this.$.networkSelectLogin.setCrOncStrings();
    this.i18nUpdateLocale();
  },

  /**
   * Returns element of the network list selected by the query.
   * Used to simplify testing.
   * @param {string} query
   * @return {CrNetworkList.CrNetworkListItemType}
   */
  getNetworkListItemWithQueryForTest: function(query) {
    let networkList =
        this.$.networkSelectLogin.$$('#networkSelect').getNetworkListForTest();
    assert(networkList);
    return networkList.querySelector(query);
  },

  /**
   * Called after dialog is shown. Refreshes the list of the networks.
   * @private
   */
  onShown_: function() {
    this.async(function() {
      this.$.networkSelectLogin.refresh();
      this.$.networkSelectLogin.focus();
    }.bind(this));
  },

  /**
   * Next button click handler.
   * @private
   */
  onNextClicked_: function() {
    chrome.send('login.NetworkScreen.userActed', ['continue']);
  },

  /**
   * Back button click handler.
   * @private
   */
  onBackClicked_: function() {
    chrome.send('login.NetworkScreen.userActed', ['back']);
  },

  /**
   * Updates custom elements on network list when demo mode setup properties
   * changed.
   * @private
   */
  onDemoModeSetupChanged_: function() {
    this.$.networkSelectLogin.isOfflineDemoModeSetup =
        this.isDemoModeSetup && this.offlineDemoModeEnabled;
  },
});
