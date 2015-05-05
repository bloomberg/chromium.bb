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

/** @typedef {chrome.networkingPrivate.NetworkStateProperties} */
var NetworkStateProperties;

Polymer('cr-settings-internet-detail-page', {
  publish: {
    /**
     * ID of the page.
     *
     * @attribute PAGE_ID
     * @const {string}
     */
    PAGE_ID: 'internet-detail',

    /**
     * Route for the page.
     *
     * @attribute route
     * @type {string}
     * @default ''
     */
    route: '',

    /**
     * Whether the page is a subpage.
     *
     * @attribute subpage
     * @type {boolean}
     * @default false
     */
    subpage: false,

    /**
     * Title for the page header and navigation menu.
     *
     * @attribute pageTitle
     * @type {string}
     */
    pageTitle: loadTimeData.getString('internetDetailPageTitle'),

    /**
     * Name of the 'core-icon' to show. TODO(stevenjb): Update this with the
     * icon for the network.
     *
     * @attribute icon
     * @type {string}
     * @default 'settings-ethernet'
     */
    icon: 'settings-ethernet',

    /**
     * The network GUID to display details for.
     *
     * @attribute guid
     * @type {string}
     * @default ''
     */
    guid: '',

    /**
     * The current state for the network matching |guid|.
     *
     * @attribute networkState
     * @type {?CrOncDataElement}
     * @default null
     */
    networkState: null,
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
  guidChanged: function() {
    this.getNetworkDetails_();
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
   * @param {!NetworkStateProperties} state The network state properties.
   * @private
   */
  getPropertiesCallback_: function(state) {
    this.networkState = CrOncDataElement.create(state);
  },

  /**
   * @param {?CrOncDataElement} state The network state properties.
   * @return {string} The text to display for the network name.
   * @private
   */
  getStateName_: function(state) {
    return state && state.data.Name;
  },

  /**
   * @param {?CrOncDataElement} state The network state properties.
   * @return {string} The text to display for the network connection state.
   * @private
   */
  getStateText_: function(state) {
    // TODO(stevenjb): Localize.
    return state && state.data.ConnectionState;
  },

  /**
   * @param {?CrOncDataElement} state The network state properties.
   * @param {string} property The property name.
   * @return {string} The text to display for the property, including the label.
   * @private
   */
  getProperty_: function(state, property) {
    if (!state)
      return '';
    // TODO(stevenjb): Localize.
    var value = state.getProperty(property) || '';
    return property + ': ' + value;
  },

  /**
   * @param {?CrOncDataElement} state The network state properties.
   * @return {boolean} Whether or not the state is connected.
   * @private
   */
  isConnectedState_: function(state) {
    return state && state.connected();
  },

  /**
   * @param {?CrOncDataElement} state The network state properties.
   * @return {boolean} Whether or not the network can be connected.
   * @private
   */
  canConnect_: function(state) {
    return state && state.data.Type != 'Ethernet' && state.disconnected();
  },

  /**
   * @param {?CrOncDataElement} state The network state properties.
   * @return {boolean} Whether or not the network can be disconnected.
   * @private
   */
  canDisconnect_: function(state) {
    return state && state.data.Type != 'Ethernet' && !state.disconnected();
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
