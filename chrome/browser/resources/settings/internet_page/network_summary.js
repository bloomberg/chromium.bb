// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, WiMAX, and VPN.
 */

/**
 * @typedef {{
 *   Ethernet: (!CrOnc.DeviceStateProperties|undefined),
 *   WiFi: (!CrOnc.DeviceStateProperties|undefined),
 *   Cellular: (!CrOnc.DeviceStateProperties|undefined),
 *   WiMAX: (!CrOnc.DeviceStateProperties|undefined),
 *   VPN: (!CrOnc.DeviceStateProperties|undefined)
 * }}
 */
var DeviceStateObject;

/**
 * @typedef {{
 *   Ethernet: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   WiFi: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   Cellular: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   WiMAX: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   VPN: (Array<!CrOnc.NetworkStateProperties>|undefined)
 * }}
 */
var NetworkStateListObject;

Polymer({
  is: 'network-summary',

  behaviors: [CrPolicyNetworkBehavior],

  properties: {
    /**
     * Highest priority connected network or null.
     * @type {?CrOnc.NetworkStateProperties}
     */
    defaultNetwork: {
      type: Object,
      value: null,
      notify: true,
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {!NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * The device state for each network device type. We initialize this to
     * include a disabled WiFi type since WiFi is always present. This reduces
     * the amount of visual change on first load.
     * @private {!DeviceStateObject}
     */
    deviceStates: {
      type: Object,
      value: function() {
        return {
          WiFi: {Type: CrOnc.Type.WI_FI, State: CrOnc.DeviceState.DISABLED},
        };
      },
      notify: true,
    },

    /**
     * Array of active network states, one per device type. Initialized to
     * include a default WiFi state (see deviceStates comment).
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    activeNetworkStates_: {
      type: Array,
      value: function() {
        return [{GUID: '', Type: CrOnc.Type.WI_FI}];
      },
    },

    /**
     * List of network state data for each network type.
     * @private {!NetworkStateListObject}
     */
    networkStateLists_: {
      type: Object,
      value: function() {
        return {WiFi: []};
      },
    },
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworkListChanged event.
   * @private {?function(!Array<string>)}
   */
  networkListChangedListener_: null,

  /**
   * Listener function for chrome.networkingPrivate.onDeviceStateListChanged
   * event.
   * @private {?function(!Array<string>)}
   */
  deviceStateListChangedListener_: null,

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @private {?function(!Array<string>)}
   */
  networksChangedListener_: null,

  /**
   * Set of GUIDs identifying active networks, one for each type.
   * @private {?Set<string>}
   */
  activeNetworkIds_: null,

  /** @override */
  attached: function() {
    this.getNetworkLists_();

    this.networkListChangedListener_ = this.networkListChangedListener_ ||
        this.onNetworkListChangedEvent_.bind(this);
    this.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);

    this.deviceStateListChangedListener_ =
        this.deviceStateListChangedListener_ ||
        this.onDeviceStateListChangedEvent_.bind(this);
    this.networkingPrivate.onDeviceStateListChanged.addListener(
        this.deviceStateListChangedListener_);

    this.networksChangedListener_ = this.networksChangedListener_ ||
        this.onNetworksChangedEvent_.bind(this);
    this.networkingPrivate.onNetworksChanged.addListener(
        this.networksChangedListener_);
  },

  /** @override */
  detached: function() {
    this.networkingPrivate.onNetworkListChanged.removeListener(
        assert(this.networkListChangedListener_));

    this.networkingPrivate.onDeviceStateListChanged.removeListener(
        assert(this.deviceStateListChangedListener_));

    this.networkingPrivate.onNetworksChanged.removeListener(
        assert(this.networksChangedListener_));
  },

  /**
   * networkingPrivate.onNetworkListChanged event callback.
   * @private
   */
  onNetworkListChangedEvent_: function() {
    this.getNetworkLists_();
  },

  /**
   * networkingPrivate.onDeviceStateListChanged event callback.
   * @private
   */
  onDeviceStateListChangedEvent_: function() {
    this.getNetworkLists_();
  },

  /**
   * networkingPrivate.onNetworksChanged event callback.
   * @param {!Array<string>} networkIds The list of changed network GUIDs.
   * @private
   */
  onNetworksChangedEvent_: function(networkIds) {
    if (!this.activeNetworkIds_)
      return;  // Initial list of networks not received yet.
    networkIds.forEach(function(id) {
      if (this.activeNetworkIds_.has(id)) {
        this.networkingPrivate.getState(
            id, this.getActiveStateCallback_.bind(this, id));
      }
    }, this);
  },

  /**
   * networkingPrivate.getState event callback for an active state.
   * @param {string} id The id of the requested state.
   * @param {!CrOnc.NetworkStateProperties} state
   * @private
   */
  getActiveStateCallback_: function(id, state) {
    if (chrome.runtime.lastError) {
      var message = chrome.runtime.lastError.message;
      if (message != 'Error.NetworkUnavailable') {
        console.error(
            'Unexpected networkingPrivate.getState error: ' + message +
            ' For: ' + id);
      }
      return;
    }
    // Async call, ensure id still exists.
    if (!this.activeNetworkIds_.has(id))
      return;
    if (!state) {
      this.activeNetworkIds_.delete(id);
      return;
    }
    // Find the active state for the type and update it.
    for (var i = 0; i < this.activeNetworkStates_.length; ++i) {
      if (this.activeNetworkStates_[i].type == state.type) {
        this.activeNetworkStates_[i] = state;
        return;
      }
    }
    // Not found
    console.error('Active state not found: ' + state.Name);
  },

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, activeNetworkStates, and networkStateLists once the
   * results are returned from Chrome.
   * @private
   */
  getNetworkLists_: function() {
    // First get the device states.
    this.networkingPrivate.getDeviceStates(deviceStates => {
      // Second get the network states.
      this.getNetworkStates_(deviceStates);
    });
  },

  /**
   * Requests the list of network states from Chrome. Updates
   * activeNetworkStates and networkStateLists once the results are returned
   * from Chrome.
   * @param {!Array<!CrOnc.DeviceStateProperties>} deviceStates
   * @private
   */
  getNetworkStates_: function(deviceStates) {
    var filter = {
      networkType: CrOnc.Type.ALL,
      visible: true,
      configured: false
    };
    this.networkingPrivate.getNetworks(filter, networkStates => {
      this.updateNetworkStates_(networkStates, deviceStates);
    });
  },

  /**
   * Called after network states are received from getNetworks.
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates The state
   *     properties for all visible networks.
   * @param {!Array<!CrOnc.DeviceStateProperties>} deviceStates
   * @private
   */
  updateNetworkStates_: function(networkStates, deviceStates) {
    var newDeviceStates = /** @type {!DeviceStateObject} */ ({});
    for (var i = 0; i < deviceStates.length; ++i) {
      var state = deviceStates[i];
      newDeviceStates[state.Type] = state;
    }

    // Clear any current networks.
    var activeNetworkStatesByType =
        /** @type {!Map<string, !CrOnc.NetworkStateProperties>} */ (new Map);

    // Complete list of states by type.
    /** @type {!NetworkStateListObject} */ var newNetworkStateLists = {
      Ethernet: [],
      Tether: [],
      WiFi: [],
      Cellular: [],
      WiMAX: [],
      VPN: [],
    };

    var firstConnectedNetwork = null;
    networkStates.forEach(function(networkState) {
      var type = networkState.Type;
      if (!activeNetworkStatesByType.has(type)) {
        activeNetworkStatesByType.set(type, networkState);
        if (!firstConnectedNetwork && networkState.Type != CrOnc.Type.VPN &&
            networkState.ConnectionState == CrOnc.ConnectionState.CONNECTED) {
          firstConnectedNetwork = networkState;
        }
      }
      newNetworkStateLists[type].push(networkState);
    }, this);

    this.defaultNetwork = firstConnectedNetwork;

    // Create a VPN entry in deviceStates if there are any VPN networks.
    if (newNetworkStateLists.VPN && newNetworkStateLists.VPN.length > 0) {
      newDeviceStates.VPN = {
        Type: CrOnc.Type.VPN,
        State: CrOnc.DeviceState.ENABLED
      };
    }

    // Push the active networks onto newActiveNetworkStates in order based on
    // device priority, creating an empty state for devices with no networks.
    var newActiveNetworkStates = [];
    this.activeNetworkIds_ = new Set;
    var orderedDeviceTypes = [
      CrOnc.Type.ETHERNET, CrOnc.Type.WI_FI, CrOnc.Type.CELLULAR,
      CrOnc.Type.TETHER, CrOnc.Type.WI_MAX, CrOnc.Type.VPN
    ];
    for (var i = 0; i < orderedDeviceTypes.length; ++i) {
      var type = orderedDeviceTypes[i];
      var device = newDeviceStates[type];
      if (!device)
        continue;  // The technology for this device type is unavailable.

      // If both 'Tether' and 'Cellular' technologies exist, merge the network
      // lists and do not add an active network for 'Tether' so that there is
      // only one 'Mobile data' section / subpage.
      if (type == CrOnc.Type.TETHER && newDeviceStates[CrOnc.Type.CELLULAR]) {
        newNetworkStateLists[CrOnc.Type.CELLULAR] =
            newNetworkStateLists[CrOnc.Type.CELLULAR].concat(
                newNetworkStateLists[CrOnc.Type.TETHER]);
        continue;
      }

      // Note: The active state for 'Cellular' may be a Tether network if both
      // types are enabled but no Cellular network exists (edge case).
      var state = this.getActiveStateForType_(activeNetworkStatesByType, type);
      if (state.Source === undefined &&
          device.State == CrOnc.DeviceState.PROHIBITED) {
        // Prohibited technologies are enforced by the device policy.
        state.Source = CrOnc.Source.DEVICE_POLICY;
      }
      newActiveNetworkStates.push(state);
      this.activeNetworkIds_.add(state.GUID);
    }

    this.deviceStates = newDeviceStates;
    this.networkStateLists_ = newNetworkStateLists;
    // Set activeNetworkStates last to rebuild the dom-repeat.
    this.activeNetworkStates_ = newActiveNetworkStates;
  },

  /**
   * Returns the active network state for |type| or a default network state.
   * If there is no 'Cellular' network, return the active 'Tether' network if
   * any since the two types are represented by the same section / subpage.
   * @param {!Map<string, !CrOnc.NetworkStateProperties>} activeStatesByType
   * @param {string} type
   * @return {!CrOnc.NetworkStateProperties|undefined}
   */
  getActiveStateForType_: function(activeStatesByType, type) {
    var activeState = activeStatesByType.get(type);
    if (!activeState && type == CrOnc.Type.CELLULAR)
      activeState = activeStatesByType.get(CrOnc.Type.TETHER);
    return activeState || {GUID: '', Type: type};
  },
});
