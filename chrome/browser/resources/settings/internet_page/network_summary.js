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

/** @typedef {chrome.networkingPrivate.NetworkStateProperties} */
var NetworkStateProperties;

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
 *   Ethernet: (CrOncDataElement|undefined),
 *   WiFi: (CrOncDataElement|undefined),
 *   Cellular: (CrOncDataElement|undefined),
 *   WiMAX: (CrOncDataElement|undefined),
 *   VPN: (CrOncDataElement|undefined)
 * }}
 */
var NetworkStateObject;

/**
 * @typedef {{
 *   Ethernet: (Array<CrOncDataElement>|undefined),
 *   WiFi: (Array<CrOncDataElement>|undefined),
 *   Cellular: (Array<CrOncDataElement>|undefined),
 *   WiMAX: (Array<CrOncDataElement>|undefined),
 *   VPN: (Array<CrOncDataElement>|undefined)
 * }}
 */
var NetworkStateListObject;

/** @const {!Array<string>} */
var NETWORK_TYPES = ['Ethernet', 'WiFi', 'Cellular', 'WiMAX', 'VPN'];

Polymer('cr-network-summary', {
  publish: {
    /**
     * The device state for each network device type.
     *
     * @attribute deviceStates
     * @type {?DeviceStateObject}
     * @default null
     */
    deviceStates: null,

    /**
     * Network state data for each network type.
     *
     * @attribute networkStates
     * @type {?NetworkStateObject}
     * @default null
     */
    networkStates: null,

    /**
     * List of network state data for each network type.
     *
     * @attribute networkStateLists
     * @type {?NetworkStateListObject}
     * @default null
     */
    networkStateLists: null,
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
  created: function() {
    this.deviceStates = {};
    this.networkStates = {};
    this.networkStateLists = {};
    this.networkIds_ = {};
  },

  /** @override */
  attached: function() {
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
   * Event triggered when the WiFi cr-network-summary-item is expanded.
   * @param {!{detail: {expanded: boolean, type: string}}} event
   * @private
   */
  onWiFiExpanded_: function(event) {
    this.getNetworkStates_();  // Get the latest network states (only).
    chrome.networkingPrivate.requestNetworkScan();
  },

  /**
   * Event triggered when a cr-network-summary-item is selected.
   * @param {!{detail: !CrOncDataElement}} event
   * @private
   */
  onSelected_: function(event) {
    var onc = event.detail;
    if (onc.disconnected()) {
      this.connectToNetwork_(onc);
      return;
    }
    // TODO(stevenjb): Show details for connected or unconfigured networks.
  },

  /**
   * Event triggered when the enabled state of a cr-network-summary-item is
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
        chrome.networkingPrivate.getState(id,
                                          this.getStateCallback_.bind(this));
      }
    }, this);
  },

  /**
   * Handles UI requests to connect to a network.
   * TODO(stevenjb): Handle Cellular activation, etc.
   * @param {!CrOncDataElement} state The network state.
   * @private
   */
  connectToNetwork_: function(state) {
    chrome.networkingPrivate.startConnect(state.data.GUID);
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
   * @param {!Array<!NetworkStateProperties>} states The state properties for
   *     all visible networks.
   * @private
   */
  getNetworksCallback_: function(states) {
    // Clear any current networks.
    this.networkIds_ = {};

    // Get the first (active) state for each type.
    var foundTypes = {};
    /** @type {!NetworkStateListObject} */ var oncNetworks = {
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
      oncNetworks[type].push(CrOncDataElement.create(state));
    }, this);

    // Set any types not found to a default value or null.
    NETWORK_TYPES.forEach(function(type) {
      if (!foundTypes[type]) {
        /** @type {NetworkStateProperties} */ var defaultState = null;
        if (this.deviceStates[type])
          defaultState = { GUID: '', Type: 'WiFi' };
        this.updateNetworkState_(type, defaultState);
      }
    }, this);

    // Set the network list for each type.
    NETWORK_TYPES.forEach(function(type) {
      this.networkStateLists[type] = oncNetworks[type];
    }, this);

    // Create a VPN entry in deviceStates if there are any VPN networks.
    if (this.networkStateLists.VPN && this.networkStateLists.VPN.length > 0)
      this.deviceStates.VPN = { Type: 'VPN', State: 'Enabled' };
  },

  /**
   * networkingPrivate.getState callback.
   * @param {!NetworkStateProperties} state The network state properties.
   * @private
   */
  getStateCallback_: function(state) {
    var id = state.GUID;
    if (!this.networkIds_[id])
      return;
    this.updateNetworkState_(state.Type, state);
  },

  /**
   * Creates a CrOncDataElement from the network state (if not null) for 'type'.
   * Sets 'networkStates[type]' which will update the cr-network-list-item
   * associated with 'type'.
   * @param {string} type The network type.
   * @param {?NetworkStateProperties} state The state properties for the network
   *     to associate with |type|. May be null if there are no networks matching
   *     |type|.
   * @private
   */
  updateNetworkState_: function(type, state) {
    this.networkStates[type] = state ? CrOncDataElement.create(state) : null;
    if (state)
      this.networkIds_[state.GUID] = true;
  },
});
})();
