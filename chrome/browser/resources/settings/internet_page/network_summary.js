// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, WiMAX, and VPN.
 */
(function() {

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
var DeviceStateProperties;

/**
 * @typedef {{
 *   Ethernet: (DeviceStateProperties|undefined),
 *   WiFi: (DeviceStateProperties|undefined),
 *   Cellular: (DeviceStateProperties|undefined),
 *   WiMAX: (DeviceStateProperties|undefined),
 *   VPN: (DeviceStateProperties|undefined)
 * }}
 */
var DeviceStateObject;

/**
 * @typedef {{
 *   Ethernet: (?CrOnc.NetworkStateProperties|undefined),
 *   WiFi: (?CrOnc.NetworkStateProperties|undefined),
 *   Cellular: (?CrOnc.NetworkStateProperties|undefined),
 *   WiMAX: (?CrOnc.NetworkStateProperties|undefined),
 *   VPN: (?CrOnc.NetworkStateProperties|undefined)
 * }}
 */
var NetworkStateObject;

/**
 * @typedef {{
 *   Ethernet: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   WiFi: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   Cellular: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   WiMAX: (Array<CrOnc.NetworkStateProperties>|undefined),
 *   VPN: (Array<CrOnc.NetworkStateProperties>|undefined)
 * }}
 */
var NetworkStateListObject;

/** @const {!Array<string>} */
var NETWORK_TYPES = [
  CrOnc.Type.ETHERNET,
  CrOnc.Type.WIFI,
  CrOnc.Type.CELLULAR,
  CrOnc.Type.WIMAX,
  CrOnc.Type.VPN
];

Polymer({
  is: 'network-summary',

  properties: {
    /**
     * The device state for each network device type.
     * @type {DeviceStateObject}
     */
    deviceStates: {
      type: Object,
      value: function() { return {}; },
    },

    /**
     * Network state data for each network type.
     * @type {NetworkStateObject}
     */
    networkStates: {
      type: Object,
      value: function() { return {}; },
    },

    /**
     * List of network state data for each network type.
     * @type {NetworkStateListObject}
     */
    networkStateLists: {
      type: Object,
      value: function() { return {}; },
    }
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworkListChanged event.
   * @type {?function(!Array<string>)}
   * @private
   */
  networkListChangedListener_: null,

  /**
   * Listener function for chrome.networkingPrivate.onDeviceStateListChanged
   * event.
   * @type {?function(!Array<string>)}
   * @private
   */
  deviceStateListChangedListener_: null,

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {?function(!Array<string>)}
   * @private
   */
  networksChangedListener_: null,

  /**
   * Dictionary of GUIDs identifying primary (active) networks for each type.
   * @type {?Object}
   * @private
   */
  networkIds_: null,

  /** @override */
  attached: function() {
    this.networkIds_ = {};

    this.getNetworkLists_();

    this.networkListChangedListener_ =
        this.onNetworkListChangedEvent_.bind(this);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);

    this.deviceStateListChangedListener_ =
        this.onDeviceStateListChangedEvent_.bind(this);
    chrome.networkingPrivate.onDeviceStateListChanged.addListener(
        this.deviceStateListChangedListener_);

    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    chrome.networkingPrivate.onNetworkListChanged.removeListener(
        this.networkListChangedListener_);

    chrome.networkingPrivate.onDeviceStateListChanged.removeListener(
        this.deviceStateListChangedListener_);

    chrome.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /**
   * Event triggered when the WiFi network-summary-item is expanded.
   * @param {!{detail: {expanded: boolean, type: string}}} event
   * @private
   */
  onWiFiExpanded_: function(event) {
    this.getNetworkStates_();  // Get the latest network states (only).
    if (event.detail.expanded)
      chrome.networkingPrivate.requestNetworkScan();
  },

  /**
   * Event triggered when a network-summary-item is selected.
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onSelected_: function(event) {
    var state = event.detail;
    if (state.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED) {
      this.connectToNetwork_(state);
      return;
    }
    MoreRouting.navigateTo('internet-detail', {guid: state.GUID});
  },

  /**
   * Event triggered when the enabled state of a network-summary-item is
   * toggled.
   * @param {!{detail: {enabled: boolean, type: string}}} event
   * @private
   */
  onDeviceEnabledToggled_: function(event) {
    if (event.detail.enabled)
      chrome.networkingPrivate.enableNetworkType(event.detail.type);
    else
      chrome.networkingPrivate.disableNetworkType(event.detail.type);
  },

  /**
   * networkingPrivate.onNetworkListChanged event callback.
   * @private
   */
  onNetworkListChangedEvent_: function() { this.getNetworkLists_(); },

  /**
   * networkingPrivate.onDeviceStateListChanged event callback.
   * @private
   */
  onDeviceStateListChangedEvent_: function() { this.getNetworkLists_(); },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    networkIds.forEach(function(id) {
      if (id in this.networkIds_) {
        chrome.networkingPrivate.getState(
            id,
            function(state) {
              if (chrome.runtime.lastError) {
                if (chrome.runtime.lastError != 'Error.NetworkUnavailable') {
                  console.error('Unexpected networkingPrivate.getState error:',
                                chrome.runtime.lastError);
                }
                return;
              }
              // Async call, ensure id still exists.
              if (!this.networkIds_[id])
                return;
              if (!state) {
                this.networkIds_[id] = undefined;
                return;
              }
              this.updateNetworkState_(state.Type, state);
            }.bind(this));
      }
    }, this);
  },

  /**
   * Handles UI requests to connect to a network.
   * TODO(stevenjb): Handle Cellular activation, etc.
   * @param {!CrOnc.NetworkStateProperties} state The network state.
   * @private
   */
  connectToNetwork_: function(state) {
    chrome.networkingPrivate.startConnect(state.GUID, function() {
      if (chrome.runtime.lastError &&
          chrome.runtime.lastError != 'connecting') {
        console.error('Unexpected networkingPrivate.startConnect error:',
                      chrome.runtime.lastError);
      }
    });
  },

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, networkStates, and networkStateLists once the
   * results are returned from Chrome.
   * @private
   */
  getNetworkLists_: function() {
    // First get the device states.
    chrome.networkingPrivate.getDeviceStates(
      function(states) {
        this.getDeviceStatesCallback_(states);
        // Second get the network states.
        this.getNetworkStates_();
      }.bind(this));
  },

  /**
   * Requests the list of network states from Chrome. Updates networkStates and
   * networkStateLists once the results are returned from Chrome.
   * @private
   */
  getNetworkStates_: function() {
    var filter = {
      networkType: 'All',
      visible: true,
      configured: false
    };
    chrome.networkingPrivate.getNetworks(
        filter, this.getNetworksCallback_.bind(this));
  },

  /**
   * networkingPrivate.getDeviceStates callback.
   * @param {!Array<!DeviceStateProperties>} states The state properties for all
   *     available devices.
   * @private
   */
  getDeviceStatesCallback_: function(states) {
    /** @type {!DeviceStateObject} */ var newStates = {};
    states.forEach(function(state) { newStates[state.Type] = state; });
    this.deviceStates = newStates;
  },

  /**
   * networkingPrivate.getNetworksState callback.
   * @param {!Array<!CrOnc.NetworkStateProperties>} states The state properties
   *     for all visible networks.
   * @private
   */
  getNetworksCallback_: function(states) {
    // Clear any current networks.
    this.networkIds_ = {};

    // Track the first (active) state for each type.
    var foundTypes = {};

    // Complete list of states by type.
    /** @type {!NetworkStateListObject} */ var networkStateLists = {
      Ethernet: [],
      WiFi: [],
      Cellular: [],
      WiMAX: [],
      VPN: []
    };

    states.forEach(function(state) {
      var type = state.Type;
      if (!foundTypes[type]) {
        foundTypes[type] = true;
        this.updateNetworkState_(type, state);
      }
      networkStateLists[type].push(state);
    }, this);

    // Set any types with a deviceState and no network to a default state,
    // and any types not found to null.
    NETWORK_TYPES.forEach(function(type) {
      if (!foundTypes[type]) {
        /** @type {CrOnc.NetworkStateProperties} */ var defaultState = null;
        if (this.deviceStates[type])
          defaultState = { GUID: '', Type: type };
        this.updateNetworkState_(type, defaultState);
      }
    }, this);

    this.networkStateLists = networkStateLists;

    // Create a VPN entry in deviceStates if there are any VPN networks.
    if (networkStateLists.VPN && networkStateLists.VPN.length > 0) {
      var vpn = { Type: CrOnc.Type.VPN, State: 'Enabled' };
      this.set('deviceStates.VPN', vpn);
    }
  },

  /**
   * Sets 'networkStates[type]' which will update the cr-network-list-item
   * associated with 'type'.
   * @param {string} type The network type.
   * @param {?CrOnc.NetworkStateProperties} state The state properties for the
   *     network to associate with |type|. May be null if there are no networks
   *     matching |type|.
   * @private
   */
  updateNetworkState_: function(type, state) {
    this.set('networkStates.' + type, state);
    if (state)
      this.networkIds_[state.GUID] = true;
  },
});
})();
