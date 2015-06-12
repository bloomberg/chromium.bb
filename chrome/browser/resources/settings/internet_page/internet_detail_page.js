// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-internet-detail' is the settings subpage containing details
 * for a network.
 *
 * @group Chrome Settings Elements
 * @element cr-settings-internet-detail
 */
(function() {

Polymer({
  is: 'cr-settings-internet-detail-page',

  properties: {
    /**
     * ID of the page.
     *
     * @attribute PAGE_ID
     * @const {string}
     */
    PAGE_ID: {
      type: String,
      value: 'internet-detail',
      readOnly: true
    },

    /**
     * Route for the page.
     */
    route: {
      type: String,
      value: ''
    },

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('internetDetailPageTitle');
      }
    },

    /**
     * Reflects the selected settings page. We use this to extract guid from
     * window.location.href when this page is navigated to. This is a
     * workaround for a bug in the 1.0 version of more-routing where
     * selected-params="{{params}}" is not correctly setting params in
     * settings_main.html. TODO(stevenjb): Remove once more-routing is fixed.
     */
    selectedPage: {
      type: String,
      value: '',
      observer: 'selectedPageChanged_'
    },

    /**
     * Name of the 'core-icon' to show. TODO(stevenjb): Update this with the
     * icon for the active internet connection.
     */
    icon: {
      type: String,
      value: 'settings-ethernet',
      readOnly: true
    },

    /**
     * The network GUID to display details for.
     */
    guid: {
      type: String,
      value: '',
      observer: 'guidChanged_',
    },

    /**
     * The current state for the network matching |guid|.
     *
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null
    },
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {?function(!Array<string>)}
   * @private
   */
  networksChangedListener_: null,

  /** @override */
  attached: function() {
    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    chrome.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /**
   * Polymer guid changed method.
   */
  guidChanged_: function() {
    if (!this.guid)
      return;
    this.getNetworkDetails_();
  },

  /**
   * Polymer guid changed method. TODO(stevenjb): Remove, see TODO above.
   */
  selectedPageChanged_: function() {
    if ((this.selectedPage && this.selectedPage.PAGE_ID) != this.PAGE_ID)
      return;
    var href = window.location.href;
    var idx = href.lastIndexOf('/');
    var guid = href.slice(idx + 1);
    this.guid = guid;
  },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    if (networkIds.indexOf(this.guid) != -1)
      this.getNetworkDetails_();
  },

  /**
   * Calls networkingPrivate.getState for this.guid.
   * @private
   */
  getNetworkDetails_: function() {
    if (!this.guid)
      return;
    chrome.networkingPrivate.getProperties(
        this.guid, this.getPropertiesCallback_.bind(this));
  },

  /**
   * networkingPrivate.getProperties callback.
   * @param {!CrOnc.NetworkStateProperties} state The network state properties.
   * @private
   */
  getPropertiesCallback_: function(state) {
    this.networkState = state;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {string} The text to display for the network name.
   * @private
   */
  getStateName_: function(state) {
    return (state && state.Name) || '';
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {string} The text to display for the network name.
   * @private
   */
  getStateClass_: function(state) {
    return this.isConnectedState_(state) ? 'connected' : '';
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function(state) {
    // TODO(stevenjb): Localize.
    return (state && state.ConnectionState) || '';
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} property The property name.
   * @return {string} The text to display for the property, including the label.
   * @private
   */
  getProperty_: function(state, property) {
    if (!state)
      return '';
    var value = this.get(property, state) || '';
    // TODO(stevenjb): Localize.
    return property + ': ' + value;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if the state is connected.
   * @private
   */
  isConnectedState_: function(state) {
    return state && state.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} Whether or not the network can be connected.
   * @private
   */
  canConnect_: function(state) {
    return state && state.Type != 'Ethernet' &&
           state.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} Whether or not the network can be disconnected.
   * @private
   */
  canDisconnect_: function(state) {
    return state && state.Type != 'Ethernet' &&
           state.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} type The network type to match.
   * @return {boolean} Whether or not the type of 'state' matches 'type'.
   * @private
   */
  isType_: function(state, type) {
    return state && state.Type == type;
  },

  /**
   * Callback when the Connect button is clicked.
   * @private
   */
  onConnectClicked_: function() {
    chrome.networkingPrivate.startConnect(this.guid);
  },

  /**
   * Callback when the Disconnect button is clicked.
   * @private
   */
  onDisconnectClicked_: function() {
    chrome.networkingPrivate.startDisconnect(this.guid);
  },
});
})();
