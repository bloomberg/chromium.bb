// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a summary of network states
 * by type: Ethernet, WiFi, Cellular, WiMAX, and VPN.
 */

/**
 * @typedef {{
 *   Ethernet: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   WiFi: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   Cellular: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   WiMAX: (Array<!CrOnc.NetworkStateProperties>|undefined),
 *   VPN: (Array<!CrOnc.NetworkStateProperties>|undefined)
 * }}
 */
let NetworkStateListObject;

Polymer({
  is: 'network-summary',

  behaviors: [
    CrNetworkListenerBehavior,
    CrPolicyNetworkBehavior,
  ],

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
     * @private {!Object<!OncMojo.DeviceStateProperties>}
     */
    deviceStates: {
      type: Object,
      value: function() {
        const result = {};
        result[chromeos.networkConfig.mojom.NetworkType.kWiFi] = {
          deviceState: chromeos.networkConfig.mojom.DeviceStateType.kDisabled,
          type: chromeos.networkConfig.mojom.NetworkType.kWiFi,
        };
        return result;
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
   * This UI will use both the networkingPrivate extension API and the
   * networkConfig mojo API until we provide all of the required functionality
   * in networkConfig. TODO(stevenjb): Remove use of networkingPrivate api.
   * @private {?chromeos.networkConfig.mojom.CrosNetworkConfigProxy}
   */
  networkConfigProxy_: null,

  /**
   * Set of GUIDs identifying active networks, one for each type.
   * @private {?Set<string>}
   */
  activeNetworkIds_: null,

  /** @override */
  created: function() {
    this.networkConfigProxy_ =
        network_config.MojoInterfaceProviderImpl.getInstance()
            .getMojoServiceProxy();
  },

  /** @override */
  attached: function() {
    this.getNetworkLists_();
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   */
  onActiveNetworksChanged: function(networks) {
    if (!this.activeNetworkIds_) {
      // Initial list of networks not received yet.
      return;
    }
    networks.forEach(network => {
      const id = network.guid;
      if (this.activeNetworkIds_.has(id)) {
        this.networkingPrivate.getState(
            id, this.getActiveStateCallback_.bind(this, id));
      }
    });
  },

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged: function() {
    this.getNetworkLists_();
  },

  /** CrosNetworkConfigObserver impl */
  onDeviceStateListChanged: function() {
    this.getNetworkLists_();
  },

  /**
   * networkingPrivate.getState event callback for an active state.
   * @param {string} id The id of the requested state.
   * @param {!CrOnc.NetworkStateProperties} state
   * @private
   */
  getActiveStateCallback_: function(id, state) {
    if (chrome.runtime.lastError) {
      const message = chrome.runtime.lastError.message;
      if (message != 'Error.NetworkUnavailable' &&
          message != 'Error.InvalidNetworkGuid') {
        console.error(
            'Unexpected networkingPrivate.getState error: ' + message +
            ' For: ' + id);
        return;
      }
    }
    // Async call, ensure id still exists.
    if (!this.activeNetworkIds_.has(id)) {
      return;
    }
    if (!state) {
      this.activeNetworkIds_.delete(id);
      return;
    }
    // Find the active state for the type and update it.
    const idx =
        this.activeNetworkStates_.findIndex((s) => s.Type == state.Type);
    if (idx == -1) {
      console.error('Active state not found: ' + state.Name);
      return;
    }
    this.set(['activeNetworkStates_', idx], state);
  },

  /**
   * Requests the list of device states and network states from Chrome.
   * Updates deviceStates, activeNetworkStates, and networkStateLists once the
   * results are returned from Chrome.
   * @private
   */
  getNetworkLists_: function() {
    // First get the device states.
    this.networkConfigProxy_.getDeviceStateList().then(response => {
      // Second get the network states.
      this.getNetworkStates_(response.result);
    });
  },

  /**
   * Requests the list of network states from Chrome. Updates
   * activeNetworkStates and networkStateLists once the results are returned
   * from Chrome.
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStateList
   * @private
   */
  getNetworkStates_: function(deviceStateList) {
    const filter = {
      networkType: CrOnc.Type.ALL,
      visible: true,
      configured: false
    };
    this.networkingPrivate.getNetworks(filter, networkStates => {
      this.updateNetworkStates_(networkStates, deviceStateList);
    });
  },

  /**
   * Called after network states are received from getNetworks.
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates The state
   *     properties for all visible networks.
   * @param {!Array<!OncMojo.DeviceStateProperties>} deviceStateList
   * @private
   */
  updateNetworkStates_: function(networkStates, deviceStateList) {
    const mojom = chromeos.networkConfig.mojom;
    const newDeviceStates = {};
    for (const device of deviceStateList) {
      newDeviceStates[device.type] = device;
    }

    // Clear any current networks.
    const activeNetworkStatesByType =
        /** @type {!Map<string, !CrOnc.NetworkStateProperties>} */ (new Map);

    // Complete list of states by type.
    /** @type {!NetworkStateListObject} */ const newNetworkStateLists = {
      Ethernet: [],
      Tether: [],
      WiFi: [],
      Cellular: [],
      WiMAX: [],
      VPN: [],
    };

    let firstConnectedNetwork = null;
    networkStates.forEach(function(networkState) {
      const type = networkState.Type;
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
      newDeviceStates[mojom.NetworkType.kVPN] = {
        type: mojom.NetworkType.kVPN,
        deviceState: chromeos.networkConfig.mojom.DeviceStateType.kEnabled,
      };
    }

    // Push the active networks onto newActiveNetworkStates in order based on
    // device priority, creating an empty state for devices with no networks.
    const newActiveNetworkStates = [];
    this.activeNetworkIds_ = new Set;
    const orderedDeviceTypes = [
      mojom.NetworkType.kEthernet,
      mojom.NetworkType.kWiFi,
      mojom.NetworkType.kCellular,
      mojom.NetworkType.kTether,
      mojom.NetworkType.kWiMAX,
      mojom.NetworkType.kVPN,
    ];
    for (const deviceType of orderedDeviceTypes) {
      const device = newDeviceStates[deviceType];
      if (!device) {
        continue;
      }  // The technology for this device type is unavailable.

      // If both 'Tether' and 'Cellular' technologies exist, merge the network
      // lists and do not add an active network for 'Tether' so that there is
      // only one 'Mobile data' section / subpage.
      if (deviceType == mojom.NetworkType.kTether &&
          newDeviceStates[mojom.NetworkType.kCellular]) {
        newNetworkStateLists[CrOnc.Type.CELLULAR] =
            newNetworkStateLists[CrOnc.Type.CELLULAR].concat(
                newNetworkStateLists[CrOnc.Type.TETHER]);
        continue;
      }

      // Note: The active state for 'Cellular' may be a Tether network if both
      // types are enabled but no Cellular network exists (edge case).
      const networkState = this.getActiveStateForType_(
          activeNetworkStatesByType, OncMojo.getNetworkTypeString(deviceType));
      if (networkState.Source === undefined &&
          device.deviceState ==
              chromeos.networkConfig.mojom.DeviceStateType.kProhibited) {
        // Prohibited technologies are enforced by the device policy.
        networkState.Source = CrOnc.Source.DEVICE_POLICY;
      }
      newActiveNetworkStates.push(networkState);
      this.activeNetworkIds_.add(networkState.GUID);
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
    let activeState = activeStatesByType.get(type);
    if (!activeState && type == CrOnc.Type.CELLULAR) {
      activeState = activeStatesByType.get(CrOnc.Type.TETHER);
    }
    return activeState || {GUID: '', Type: type};
  },

  /**
   * @param {string} type
   * @param {!Object<!OncMojo.DeviceStateProperties>} deviceStates
   * @return {!OncMojo.DeviceStateProperties|undefined}
   */
  getDeviceState_: function(type, deviceStates) {
    return this.deviceStates[OncMojo.getNetworkTypeFromString(type)];
  },
});
