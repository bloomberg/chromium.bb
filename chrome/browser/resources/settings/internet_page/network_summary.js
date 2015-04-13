// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, WiMAX, and VPN.
 */
Polymer('cr-network-summary', {
  publish: {
    /**
     * Network state data for each network type.
     *
     * @attribute networkStates
     * @type {{
     *   Ethernet: (CrOncDataElement|undefined),
     *   WiFi: (CrOncDataElement|undefined),
     *   Cellular: (CrOncDataElement|undefined),
     *   WiMAX: (CrOncDataElement|undefined),
     *   VPN: (CrOncDataElement|undefined)
     * }}
     * @default {}
     */
    networkStates: null,
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworkListChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  listChangedListener_: null,

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networksChangedListener_: null,

  /**
   * Dictionary of GUIDs identifying primary (active) networks for each type.
   * @type {Object}
   * @private
   */
  networkIds_: {},

  /** @override */
  created: function() {
    this.networkStates = {};
  },

  /** @override */
  attached: function() {
    this.getNetworks_();

    this.listChangedListener_ = this.onNetworkListChangedEvent_.bind(this);
    chrome.networkingPrivate.onNetworkListChanged.addListener(
        this.listChangedListener_);

    this.networksChangedListener_ = this.onNetworksChangedEvent_.bind(this);
    chrome.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    chrome.networkingPrivate.onNetworkListChanged.removeListener(
        this.listChangedListener_);

    chrome.networkingPrivate.onNetworksChanged.removeListener(
        this.networksChangedListener_);
  },

  /**
   * @private
   */
  onNetworkListChangedEvent_: function() {
    this.getNetworks_();
  },

  /**
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

  /** @private */
  getNetworks_: function() {
    var filter = {
      networkType: 'All',
      visible: true,
      configured: false
    };
    chrome.networkingPrivate.getNetworks(filter,
                                         this.getNetworksCallback_.bind(this));
  },

  /**
   * @param {!Array<!chrome.networkingPrivate.NetworkStateProperties>} states
   *     The state properties for all networks.
   * @private
   */
  getNetworksCallback_: function(states) {
    // Clear all active networks.
    this.networkIds_ = {};

    // Get the first (active) state for each type.
    var foundTypes = {};
    states.forEach(function(state) {
      var type = state.Type;
      if (!foundTypes[type]) {
        foundTypes[type] = true;
        this.updateNetworkState_(type, state);
      }
    }, this);

    // Set any types not found to null. TODO(stevenjb): Support types that are
    // disabled but available with no active network.
    var types = ['Ethernet', 'WiFi', 'Cellular', 'WiMAX', 'VPN'];
    types.forEach(function(type) {
      if (!foundTypes[type])
        this.updateNetworkState_(type, null);
    }, this);
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkStateProperties} state The state
   *     properties for the network.
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
   * @param {chrome.networkingPrivate.NetworkStateProperties} state The state
   *     properties for the network to associate with |type|. May be null if
   *     there are no networks matching |type|.
   * @private
   */
  updateNetworkState_: function(type, state) {
    this.networkStates[type] = state ? CrOncDataElement.create(state) : null;
    if (state)
      this.networkIds_[state.GUID] = true;
  },
});
