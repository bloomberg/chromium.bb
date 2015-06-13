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

Polymer({
  is: 'cr-settings-internet-detail-page',

  properties: {
    /**
     * ID of the page.
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
     * True if the Advanced section is expanded.
     */
    advancedExpanded: {
      type: Boolean,
      value: false
    },

    /**
     * True if the device section is expanded.
     */
    deviceExpanded: {
      type: Boolean,
      value: false
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
     * @type {?CrOnc.NetworkStateProperties}
     */
    networkState: {
      type: Object,
      value: null,
      observer: 'networkStateChanged_'
    },

    /**
     * The network AutoConnect state.
     */
    autoConnect: {
      type: Boolean,
      value: false,
      observer: 'autoConnectChanged_'
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
   * Polymer networkState changed method.
   */
  networkStateChanged_: function() {
    if (!this.networkState)
      return;
    // Update the autoConnect property if it has changed.
    var autoConnect =
        CrOnc.getActiveTypeValue(this.networkState, 'AutoConnect');
    if (autoConnect != this.autoConnect)
      this.autoConnect = autoConnect;
  },

  /**
   * Polymer autoConnect changed method.
   */
  autoConnectChanged_: function() {
    if (!this.networkState || !this.guid)
      return;
    var onc = { Type: this.networkState.Type };
    CrOnc.setTypeProperty(onc, 'AutoConnect', this.autoConnect);
    chrome.networkingPrivate.setProperties(this.guid, onc);
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

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function(state) {
    /** @type {!Array<string>} */ var fields = [];
    if (!state)
      return fields;
    if (state.Type == 'Cellular') {
      fields = fields.concat([
        'Cellular.ActivationState',
        'Cellular.RoamingState',
        'RestrictedConnectivity',
        'Cellular.ServingOperator.Name',
      ]);
    }
    return fields;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {!Array<string>} The fields to display in the Advanced section.
   * @private
   */
  getAdvancedFields_: function(state) {
    /** @type {!Array<string>} */ var fields = [];
    if (!state)
      return fields;
    fields.push('MacAddress');
    if (state.Type == 'Cellular') {
      fields = fields.concat([
        'Cellular.Carrier',
        'Cellular.Family',
        'Cellular.NetworkTechnology',
        'Cellular.ServingOperator.Code'
      ]);
    }

    if (state.Type == 'WiFi') {
      fields = fields.concat([
        'WiFi.SSID',
        'WiFi.BSSID',
        'WiFi.Security',
        'WiFi.SignalStrength',
        'WiFi.Frequency'
      ]);
    }
    return fields;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {!Array<string>} The fields to display in the device section.
   * @private
   */
  getDeviceFields_: function(state) {
    /** @type {!Array<string>} */ var fields = [];
    if (!state)
      return fields;
    if (state.Type == 'Cellular') {
      fields = fields.concat([
        'Cellular.HomeProvider.Name',
        'Cellular.HomeProvider.Country',
        'Cellular.HomeProvider.Code',
        'Cellular.Manufacturer',
        'Cellular.ModelID',
        'Cellular.FirmwareRevision',
        'Cellular.HardwareRevision',
        'Cellular.ESN',
        'Cellular.ICCID',
        'Cellular.IMEI',
        'Cellular.IMSI',
        'Cellular.MDN',
        'Cellular.MEID',
        'Cellular.MIN',
        'Cellular.PRLVersion',
      ]);
    }
    return fields;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if there are any advanced fields to display.
   * @private
   */
  hasAdvancedOrDeviceFields_: function(state) {
    return this.getAdvancedFields_(state).length > 0 || this.hasDeviceFields_();
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if there are any device fields to display.
   * @private
   */
  hasDeviceFields_: function(state) {
    var fields = this.getDeviceFields_(state);
    return fields.length > 0;
  }
});
