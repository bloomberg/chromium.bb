// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design OOBE.
 */

Polymer({
  is: 'oobe-welcome-md',

  properties: {
    /**
     * Currently selected system language.
     */
    currentLanguage: {
      type: String,
      value: 'English (US)',
    },

    /**
     * Flag that switches Welcome screen to Network Selection screen.
     */
    networkSelectionScreenShown: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * GUID of the user-selected network. It is remembered after user taps on
   * network entry. After we receive event "connected" on this network,
   * OOBE will proceed.
   */
  networkLastSelectedGuid_: '',

  /**
   * Sets proper focus.
   */
  focus: function() {
    this.$.welcomeNextButton.focus();
  },

  /**
   * Handles "visible" event.
   * @private
   */
  onAnimationFinish_: function() {
    this.focus();
  },

  /**
   * Returns custom items for network selector.
   */
  _getNetworkCustomItems: function() {
    var self = this;
    return [
      {
        customItemName: 'proxySettingsMenuName',
        polymerIcon: 'oobe-welcome:add',
        customData: {
          onTap: function() { self.OpenProxySettingsDialog_(); },
        },
      },
      {
        customItemName: 'addWiFiNetworkMenuName',
        polymerIcon: 'oobe-welcome:add',
        customData: {
          onTap: function() { self.OpenAddWiFiNetworkDialog_(); },
        },
      },
      {
        customItemName: 'addMobileNetworkMenuName',
        polymerIcon: 'oobe-welcome:add',
        customData: {
          onTap: function() { self.OpenAddWiFiNetworkDialog_(); },
        },
      },
    ];
  },

  /**
   * Handle "Next" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeNextButtonClicked_: function() {
    this.networkSelectionScreenShown = true;
  },

  /**
   * Handle Networwork Setup screen "Proxy settings" button.
   *
   * @private
   */
  OpenProxySettingsDialog_: function(item) {
    chrome.send('launchProxySettingsDialog');
  },

  /**
   * Handle Networwork Setup screen "Add WiFi network" button.
   *
   * @private
   */
  OpenAddWiFiNetworkDialog_: function(item) {
    chrome.send('launchAddWiFiNetworkDialog');
  },

  /**
   * Handle Networwork Setup screen "Add cellular network" button.
   *
   * @private
   */
  OpenAddMobileNetworkDialog_: function(item) {
    chrome.send('launchAddMobileNetworkDialog');
  },

  /**
   * This is called when network setup is done.
   *
   * @private
   */
  onSelectedNetworkConnected_: function() {
    $('oobe-connect').hidden = false;
    $('oobe-welcome-md').hidden = true;
  },

  /**
   * This gets called when a network enters the 'Connected' state.
   *
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onNetworkConnected_: function(event) {
    var state = event.detail;
    if (state.GUID != this.networkLastSelectedGuid_)
      return;

    this.onSelectedNetworkConnected_();
  },

  /**
   * This is called when user taps on network entry in networks list.
   *
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onNetworkListNetworkItemSelected_: function(event) {
    var state = event.detail;
    // If user has not previously made a selection and selected network
    // is already connected, just continue to the next screen.
    if (this.networkLastSelectedGuid_ == '' &&
        state.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
      this.onSelectedNetworkConnected_();
    }

    // If user has previously selected another network, there
    // is pending connection attempt. So even if new selection is currently
    // connected, it may get disconnected at any time.
    // So just send one more connection request to cancel current attempts.
    this.networkLastSelectedGuid_ = state.GUID;

    var self = this;
    var networkStateCopy = Object.assign({}, state);

    chrome.networkingPrivate.startConnect(state.GUID, function() {
      var lastError = chrome.runtime.lastError;
      if (!lastError)
        return;

      if (lastError.message == 'connected') {
        self.onNetworkConnected_({'detail': networkStateCopy});
        return;
      }

      if (lastError.message != 'connecting')
        console.error('networkingPrivate.startConnect error: ' + lastError);
    });
  },

  /**
   * @param {!Event} event
   */
  onNetworkListCustomItemSelected_: function(e) {
    var itemState = e.detail;
    itemState.customData.onTap();
  },
});
