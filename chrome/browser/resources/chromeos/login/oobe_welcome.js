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
     * Currently selected system language (display name).
     */
    currentLanguage: {
      type: String,
      value: '',
    },

    /**
     * List of languages for language selector dropdown.
     * @type {!Array<OobeTypes.LanguageDsc>}
     */
    languages: {
      type: Array,
    },

    /**
     * List of keyboards for keyboard selector dropdown.
     * @type {!Array<OobeTypes.IMEDsc>}
     */
    keyboards: {
      type: Array,
    },

    /**
     * Flag that shows Welcome screen.
     */
    welcomeScreenShown: {
      type: Boolean,
      value: true,
    },

    /**
     * Flag that shows Language Selection screen.
     */
    languageSelectionScreenShown: {
      type: Boolean,
      value: false,
    },

    /**
     * Flag that shows Accessibility Options screen.
     */
    accessibilityOptionsScreenShown: {
      type: Boolean,
      value: false,
    },

    /**
     * Flag that shows Network Selection screen.
     */
    networkSelectionScreenShown: {
      type: Boolean,
      value: false,
    },

    /**
     * Flag that enables MD-OOBE.
     */
    enabled: {
      type: Boolean,
      value: false,
    },

    /**
     * Accessibility options status.
     * @type {!OobeTypes.A11yStatuses}
     */
    a11yStatus: {
      type: Object,
    },
  },

  /**
   * Hides all screens to help switching from one screen to another.
   */
  hideAllScreens_: function() {
    this.welcomeScreenShown = false;
    this.networkSelectionScreenShown = false;
    this.languageSelectionScreenShown = false;
    this.accessibilityOptionsScreenShown = false;
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
    this.hideAllScreens_();
    this.networkSelectionScreenShown = true;
  },

  /**
   * Handle "Language" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeSelectLanguageButtonClicked_: function() {
    this.hideAllScreens_();
    this.languageSelectionScreenShown = true;
  },

  /**
   * Handle "Accessibility" button for "Welcome" screen.
   *
   * @private
   */
  onWelcomeAccessibilityButtonClicked_: function() {
    this.hideAllScreens_();
    this.accessibilityOptionsScreenShown = true;
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
    chrome.send('login.NetworkScreen.userActed', ['continue']);
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

    // Duplicate asynchronous event may be delivered to some other screen,
    // so disable it.
    this.networkLastSelectedGuid_ = '';
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
   * @private
   */
  onNetworkListCustomItemSelected_: function(e) {
    var itemState = e.detail;
    itemState.customData.onTap();
  },

  /**
   * Handle language selection.
   *
   * @param {!{detail: {!OobeTypes.LanguageDsc}}} event
   * @private
   */
  onLanguageSelected_: function(event) {
    var item = event.detail;
    var languageId = item.value;
    this.screen.onLanguageSelected_(languageId);
  },

  /**
   * Handle keyboard layout selection.
   *
   * @param {!{detail: {!OobeTypes.IMEDsc}}} event
   * @private
   */
  onKeyboardSelected_: function(event) {
    var item = event.detail;
    var inputMethodId = item.value;
    this.screen.onKeyboardSelected_(inputMethodId);
  },

  /**
   * Handle "OK" button for "LanguageSelection" screen.
   *
   * @private
   */
  closeLanguageSection_: function() {
    this.hideAllScreens_();
    this.welcomeScreenShown = true;
  },

  /** ******************** Accessibility section ******************* */

  /**
   * Handle "OK" button for "Accessibility Options" screen.
   *
   * @private
   */
  closeAccessibilitySection_: function() {
    this.hideAllScreens_();
    this.welcomeScreenShown = true;
  },

  /**
   * Handle all accessibility buttons.
   * Note that each <oobe-a11y-option> has chromeMessage attribute
   * containing Chromium callback name.
   *
   * @private
   * @param {!Event} event
   */
  onA11yOptionChanged_: function(event) {
    chrome.send(
        event.currentTarget.chromeMessage, [event.currentTarget.checked]);
  },
});
