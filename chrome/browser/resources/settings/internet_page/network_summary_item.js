// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type.
 */

Polymer({
  is: 'network-summary-item',

  behaviors: [CrPolicyNetworkBehavior, I18nBehavior],

  properties: {
    /**
     * Device state for the network type. This might briefly be undefined if a
     * device becomes unavailable.
     * @type {!CrOnc.DeviceStateProperties|undefined}
     */
    deviceState: Object,

    /**
     * Network state for the active network.
     * @type {!CrOnc.NetworkStateProperties|undefined}
     */
    activeNetworkState: Object,

    /**
     * List of all network state data for the network type.
     * @type {!Array<!CrOnc.NetworkStateProperties>}
     */
    networkStateList: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {!NetworkingPrivate}
     */
    networkingPrivate: Object,
  },

  /**
   * @return {string}
   * @private
   */
  getNetworkName_: function() {
    return CrOncStrings['OncType' + this.activeNetworkState.Type];
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {string}
   * @private
   */
  getNetworkStateText_: function(activeNetworkState, deviceState) {
    var state = activeNetworkState.ConnectionState;
    var name = CrOnc.getNetworkName(activeNetworkState);
    if (state)
      return this.getConnectionStateText_(state, name);
    if (deviceState) {
      if (deviceState.State == CrOnc.DeviceState.ENABLING)
        return this.i18n('internetDeviceEnabling');
      if (deviceState.Type == CrOnc.Type.CELLULAR && this.deviceState.Scanning)
        return this.i18n('internetMobileSearching');
      if (deviceState.State == CrOnc.DeviceState.ENABLED)
        return CrOncStrings.networkListItemNotConnected;
    }
    return this.i18n('deviceOff');
  },

  /**
   * @param {CrOnc.ConnectionState} state
   * @param {string} name
   * @return {string}
   * @private
   */
  getConnectionStateText_: function(state, name) {
    switch (state) {
      case CrOnc.ConnectionState.CONNECTED:
        return name;
      case CrOnc.ConnectionState.CONNECTING:
        if (name)
          return CrOncStrings.networkListItemConnectingTo.replace('$1', name);
        return CrOncStrings.networkListItemConnecting;
      case CrOnc.ConnectionState.NOT_CONNECTED:
        return CrOncStrings.networkListItemNotConnected;
    }
    assertNotReached();
    return state;
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @return {boolean}
   * @private
   */
  showPolicyIndicator_: function(activeNetworkState) {
    return activeNetworkState.ConnectionState ==
        CrOnc.ConnectionState.CONNECTED ||
        this.isPolicySource(activeNetworkState.Source);
  },

  /**
   * Show the <network-siminfo> element if this is a disabled and locked
   * cellular device.
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {boolean}
   * @private
   */
  showSimInfo_: function(deviceState) {
    if (!deviceState || deviceState.Type != CrOnc.Type.CELLULAR ||
        this.deviceIsEnabled_(deviceState)) {
      return false;
    }
    return deviceState.SimPresent === false ||
        deviceState.SimLockType == CrOnc.LockType.PIN ||
        deviceState.SimLockType == CrOnc.LockType.PUK;
  },

  /**
   * Returns a NetworkProperties object for <network-siminfo> built from
   * the device properties (since there will be no active network).
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @return {!CrOnc.NetworkProperties|undefined}
   * @private
   */
  getCellularState_: function(deviceState) {
    if (!deviceState)
      return undefined;
    return {
      GUID: '',
      Type: CrOnc.Type.CELLULAR,
      Cellular: {
        SIMLockStatus: {
          LockType: deviceState.SimLockType || '',
          LockEnabled: deviceState.SimLockType != CrOnc.LockType.NONE,
        },
        SIMPresent: deviceState.SimPresent,
      },
    };
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
        return this.i18n('internetToggleTetherA11yLabel');
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
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  showDetailsIsVisible_: function(
      activeNetworkState, deviceState, networkStateList) {
    return this.deviceIsEnabled_(deviceState) &&
        (!!activeNetworkState.GUID || networkStateList.length > 0);
  },

  /**
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_: function(deviceState, networkStateList) {
    if (!deviceState)
      return false;
    var type = deviceState.Type;
    var minlen = (deviceState.Type == CrOnc.Type.WI_FI ||
                  deviceState.Type == CrOnc.Type.VPN ||
                  deviceState.Type == CrOnc.Type.TETHER) ?
        1 :
        2;
    return networkStateList.length >= minlen;
  },

  /**
   * @param {!Event} event The enable button event.
   * @private
   */
  onShowDetailsTap_: function(event) {
    if (!this.deviceIsEnabled_(this.deviceState)) {
      this.fire(
          'device-enabled-toggled',
          {enabled: true, type: this.deviceState.Type});
    } else if (this.shouldShowSubpage_(
                   this.deviceState, this.networkStateList)) {
      this.fire('show-networks', this.deviceState);
    } else if (this.activeNetworkState.GUID) {
      this.fire('show-detail', this.activeNetworkState);
    } else if (this.networkStateList.length > 0) {
      this.fire('show-detail', this.networkStateList[0]);
    }
    event.stopPropagation();
  },

  /**
   * @param {!CrOnc.NetworkStateProperties} activeNetworkState
   * @param {!CrOnc.DeviceStateProperties|undefined} deviceState
   * @param {!Array<!CrOnc.NetworkStateProperties>} networkStateList
   * @return {string}
   * @private
   */
  getDetailsA11yString_: function(
      activeNetworkState, deviceState, networkStateList) {
    if (!this.shouldShowSubpage_(deviceState, networkStateList)) {
      if (activeNetworkState.GUID) {
        return CrOnc.getNetworkName(activeNetworkState);
      } else if (networkStateList.length > 0) {
        return CrOnc.getNetworkName(networkStateList[0]);
      }
    }
    return this.i18n('OncType' + deviceState.Type);
  },

  /**
   * Event triggered when the enable button is toggled.
   * @param {!Event} event
   * @private
   */
  onDeviceEnabledTap_: function(event) {
    var deviceIsEnabled = this.deviceIsEnabled_(this.deviceState);
    var type = this.deviceState ? this.deviceState.Type : '';
    this.fire(
        'device-enabled-toggled', {enabled: !deviceIsEnabled, type: type});
    // Make sure this does not propagate to onDetailsTap_.
    event.stopPropagation();
  },
});
