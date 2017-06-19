// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about WiFi,
 * WiMAX, or virtual networks.
 */

Polymer({
  is: 'settings-internet-subpage',

  behaviors: [
    CrPolicyNetworkBehavior,
    settings.RouteObserverBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * Highest priority connected network or null. Provided by
     * settings-internet-page (but set in network-summary).
     * @type {?CrOnc.NetworkStateProperties|undefined}
     */
    defaultNetwork: Object,

    /**
     * Device state for the network type. Note: when both Cellular and Tether
     * are available this will always be set to the Cellular device state and
     * |tetherDeviceState| will be set to the Tether device state.
     * @type {!CrOnc.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * If both Cellular and Tether technologies exist, we combine the subpages
     * and set this to the device state for Tether.
     * @type {!CrOnc.DeviceStateProperties|undefined}
     */
    tetherDeviceState: Object,

    /** @type {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy: Object,

    /**
     * List of third party VPN providers.
     * @type
     *     {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>|undefined}
     */
    thirdPartyVpnProviders: Array,

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {!NetworkingPrivate}
     */
    networkingPrivate: Object,

    showSpinner: {
      type: Boolean,
      notify: true,
      value: false,
    },

    /**
     * List of all network state data for the network type.
     * @private {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Dictionary of lists of network states for third party VPNs.
     * @private {!Object<!Array<!CrOnc.NetworkStateProperties>>}
     */
    thirdPartyVpns_: {
      type: Object,
      value: function() {
        return {};
      },
    },
  },

  observers: ['updateScanning_(networkingPrivate, deviceState)'],

  /** @private {number|null} */
  scanIntervalId_: null,

  /**
   * Listener function for chrome.networkingPrivate.onNetworkListChanged event.
   * @type {?function(!Array<string>)}
   * @private
   */
  networkListChangedListener_: null,

  /** override */
  attached: function() {
    this.scanIntervalId_ = null;

    this.networkListChangedListener_ = this.networkListChangedListener_ ||
        this.onNetworkListChangedEvent_.bind(this);
    this.networkingPrivate.onNetworkListChanged.addListener(
        this.networkListChangedListener_);
  },

  /** override */
  detached: function() {
    this.stopScanning_();
    this.networkingPrivate.onNetworkListChanged.removeListener(
        assert(this.networkListChangedListener_));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.Route.INTERNET_NETWORKS) {
      this.stopScanning_();
      return;
    }
    // Clear any stale data.
    this.networkStateList_ = [];
    this.thirdPartyVpns_ = {};
    // Request the list of networks and start scanning if necessary.
    this.getNetworkStateList_();
    this.updateScanning_();
  },

  /** @private */
  updateScanning_: function() {
    if (!this.deviceState)
      return;
    if (this.deviceState.Type != CrOnc.Type.WI_FI) {
      // deviceState probably changed, re-request networks.
      this.getNetworkStateList_();
      return;
    }
    this.showSpinner = !!this.deviceState.Scanning;
    this.startScanning_();
  },

  /** @private */
  startScanning_: function() {
    if (this.scanIntervalId_ != null)
      return;
    /** @const */ var INTERVAL_MS = 10 * 1000;
    this.networkingPrivate.requestNetworkScan();
    this.scanIntervalId_ = window.setInterval(function() {
      this.networkingPrivate.requestNetworkScan();
    }.bind(this), INTERVAL_MS);
  },

  /** @private */
  stopScanning_: function() {
    if (this.scanIntervalId_ == null)
      return;
    window.clearInterval(this.scanIntervalId_);
    this.scanIntervalId_ = null;
  },

  /**
   * networkingPrivate.onNetworkListChanged event callback.
   * @private
   */
  onNetworkListChangedEvent_: function() {
    this.getNetworkStateList_();
  },

  /** @private */
  getNetworkStateList_: function() {
    if (!this.deviceState)
      return;
    var filter = {
      networkType: this.deviceState.Type,
      visible: true,
      configured: false
    };
    this.networkingPrivate.getNetworks(filter, this.onGetNetworks_.bind(this));
  },

  /**
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStates
   * @private
   */
  onGetNetworks_: function(networkStates) {
    if (!this.deviceState)
      return;  // Edge case when device states change before this callback.

    // For the Cellular/Mobile subpage, request Tether networks if available.
    if (this.deviceState.Type == CrOnc.Type.CELLULAR &&
        this.tetherDeviceState) {
      var filter = {
        networkType: CrOnc.Type.TETHER,
        visible: true,
        configured: false
      };
      this.networkingPrivate.getNetworks(filter, function(tetherNetworkStates) {
        this.networkStateList_ = networkStates.concat(tetherNetworkStates);
      }.bind(this));
      return;
    }

    // For VPNs, separate out third party VPNs.
    if (this.deviceState.Type == CrOnc.Type.VPN) {
      var builtinNetworkStates = [];
      var thirdPartyVpns = {};
      for (var i = 0; i < networkStates.length; ++i) {
        var state = networkStates[i];
        var providerType = state.VPN && state.VPN.ThirdPartyVPN &&
            state.VPN.ThirdPartyVPN.ProviderName;
        if (providerType) {
          thirdPartyVpns[providerType] = thirdPartyVpns[providerType] || [];
          thirdPartyVpns[providerType].push(state);
        } else {
          builtinNetworkStates.push(state);
        }
        networkStates = builtinNetworkStates;
        this.thirdPartyVpns_ = thirdPartyVpns;
      }
    }

    this.networkStateList_ = networkStates;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState && deviceState.State == CrOnc.DeviceState.ENABLED;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {string} onstr
   * @param {string} offstr
   * @return {string}
   * @private
   */
  getOffOnString_: function(deviceState, onstr, offstr) {
    return this.deviceIsEnabled_(deviceState) ? onstr : offstr;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    return !!deviceState && deviceState.Type != CrOnc.Type.ETHERNET &&
        deviceState.Type != CrOnc.Type.VPN;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return !!deviceState && deviceState.State != CrOnc.DeviceState.PROHIBITED;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getToggleA11yString_: function(deviceState) {
    if (!this.enableToggleIsVisible_(deviceState))
      return '';
    switch (deviceState.Type) {
      case CrOnc.Type.TETHER:
      case CrOnc.Type.CELLULAR:
        return this.i18n('internetToggleMobileA11yLabel');
      case CrOnc.Type.WI_FI:
        return this.i18n('internetToggleWiFiA11yLabel');
      case CrOnc.Type.WI_MAX:
        return this.i18n('internetToggleWiMAXA11yLabel');
    }
    assertNotReached();
    return '';
  },

  /**
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {string}
   * @private
   */
  getAddThirdPartyVpnA11yString_: function(vpnState) {
    return this.i18n('internetAddThirdPartyVPN', vpnState.ProviderName);
  },

  /**
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  allowAddConnection_: function(globalPolicy) {
    return globalPolicy && !globalPolicy.AllowOnlyPolicyNetworksToConnect;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @return {boolean}
   * @private
   */
  showAddButton_: function(deviceState, globalPolicy) {
    if (!deviceState || deviceState.Type != CrOnc.Type.WI_FI)
      return false;
    if (!this.deviceIsEnabled_(deviceState))
      return false;
    return this.allowAddConnection_(globalPolicy);
  },

  /** @private */
  onAddButtonTap_: function() {
    assert(this.deviceState);
    if (loadTimeData.getBoolean('networkSettingsConfig'))
      this.fire('show-config', {GUID: '', Type: this.deviceState.Type});
    else
      chrome.send('addNetwork', [this.deviceState.Type]);
  },

  /**
   * @param {!{model:
   *              !{item: !chrome.networkingPrivate.ThirdPartyVPNProperties},
   *        }} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    var provider = event.model.item;
    chrome.send('addNetwork', [CrOnc.Type.VPN, provider.ExtensionID]);
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  knownNetworksIsVisible_: function(deviceState) {
    return !!deviceState && deviceState.Type == CrOnc.Type.WI_FI;
  },

  /**
   * Event triggered when the known networks button is tapped.
   * @private
   */
  onKnownNetworksTap_: function() {
    assert(this.deviceState.Type == CrOnc.Type.WI_FI);
    this.fire('show-known-networks', {Type: this.deviceState.Type});
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledTap_: function(event) {
    assert(this.deviceState);
    this.fire('device-enabled-toggled', {
      enabled: !this.deviceIsEnabled_(this.deviceState),
      type: this.deviceState.Type
    });
    // Make sure this does not propagate to onDetailsTap_.
    event.stopPropagation();
  },

  /**
   * @param {!Object<!Array<!CrOnc.NetworkStateProperties>>} thirdPartyVpns
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {!Array<!CrOnc.NetworkStateProperties>}
   * @private
   */
  getThirdPartyVpnNetworks_: function(thirdPartyVpns, vpnState) {
    return thirdPartyVpns[vpnState.ProviderName] || [];
  },

  /**
   * @param {!Object<!Array<!CrOnc.NetworkStateProperties>>} thirdPartyVpns
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} vpnState
   * @return {boolean}
   * @private
   */
  haveThirdPartyVpnNetwork_: function(thirdPartyVpns, vpnState) {
    var list = this.getThirdPartyVpnNetworks_(thirdPartyVpns, vpnState);
    return !!list.length;
  },

  /**
   * Event triggered when a network list item is selected.
   * @param {!{target: HTMLElement, detail: !CrOnc.NetworkStateProperties}} e
   * @private
   */
  onNetworkSelected_: function(e) {
    assert(this.globalPolicy);
    assert(this.defaultNetwork !== undefined);
    var state = e.detail;
    e.target.blur();
    if (this.canConnect_(state, this.globalPolicy, this.defaultNetwork)) {
      this.fire('network-connect', {networkProperties: state});
      return;
    }
    this.fire('show-detail', state);
  },

  /**
   * Determines whether or not a network state can be connected to.
   * @param {!CrOnc.NetworkStateProperties} state The network state.
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @param {?CrOnc.NetworkStateProperties} defaultNetwork
   * @private
   */
  canConnect_: function(state, globalPolicy, defaultNetwork) {
    if (state.ConnectionState != CrOnc.ConnectionState.NOT_CONNECTED)
      return false;
    if (state.Type == CrOnc.Type.WI_FI && globalPolicy &&
        globalPolicy.AllowOnlyPolicyNetworksToConnect &&
        !this.isPolicySource(state.Source)) {
      return false;
    }
    if (state.Type == CrOnc.Type.VPN &&
        (!defaultNetwork ||
         defaultNetwork.ConnectionState != CrOnc.ConnectionState.CONNECTED)) {
      return false;
    }
    return true;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!CrOnc.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsVisible_: function(deviceState, tetherDeviceState) {
    return !!deviceState && deviceState.Type == CrOnc.Type.CELLULAR &&
        !!tetherDeviceState;
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!CrOnc.DeviceStateProperties|undefined} tetherDeviceState
   * @return {boolean}
   * @private
   */
  tetherToggleIsEnabled_: function(deviceState, tetherDeviceState) {
    return this.tetherToggleIsVisible_(deviceState, tetherDeviceState) &&
        this.enableToggleIsEnabled_(tetherDeviceState) &&
        tetherDeviceState.State != CrOnc.DeviceState.UNINITIALIZED;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onTetherEnabledTap_: function(event) {
    this.fire('device-enabled-toggled', {
      enabled: !this.deviceIsEnabled_(this.tetherDeviceState),
      type: CrOnc.Type.TETHER,
    });
    event.stopPropagation();
  },

  /**
   * @param {*} lhs
   * @param {*} rhs
   * @return {boolean}
   */
  isEqual_: function(lhs, rhs) {
    return lhs === rhs;
  },
});
