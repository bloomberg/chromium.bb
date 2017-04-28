// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the network state for a specific
 * type and a list of networks for that type.
 */

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
var DeviceStateProperties;

Polymer({
  is: 'network-summary-item',

  behaviors: [CrPolicyNetworkBehavior, I18nBehavior],

  properties: {
    /**
     * Device state for the network type.
     * @type {!DeviceStateProperties|undefined}
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
   * @return {string}
   * @private
   */
  getNetworkStateText_: function() {
    var network = this.activeNetworkState;
    var state = network.ConnectionState;
    var name = CrOnc.getNetworkName(network);
    if (state)
      return this.getConnectionStateText_(state, name);
    var deviceState = this.deviceState;
    if (deviceState) {
      if (deviceState.State ==
          chrome.networkingPrivate.DeviceStateType.ENABLING) {
        return this.i18n('internetDeviceEnabling');
      }
      if (deviceState.Type == CrOnc.Type.CELLULAR &&
          this.deviceState.Scanning) {
        return this.i18n('internetMobileSearching');
      }
      if (deviceState.State == chrome.networkingPrivate.DeviceStateType.ENABLED)
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
   * @return {boolean}
   * @private
   */
  showPolicyIndicator_: function() {
    var network = this.activeNetworkState;
    return network.ConnectionState == CrOnc.ConnectionState.CONNECTED ||
        this.isPolicySource(network.Source);
  },

  /**
   * Show the <network-siminfo> element if this is a disabled and locked
   * cellular device.
   * @return {boolean}
   * @private
   */
  showSimInfo_: function() {
    var device = this.deviceState;
    if (device.Type != CrOnc.Type.CELLULAR ||
        this.deviceIsEnabled_(this.deviceState)) {
      return false;
    }
    return device.SimPresent === false ||
        device.SimLockType == CrOnc.LockType.PIN ||
        device.SimLockType == CrOnc.LockType.PUK;
  },

  /**
   * Returns a NetworkProperties object for <network-siminfo> built from
   * the device properties (since there will be no active network).
   * @param {!DeviceStateProperties} deviceState
   * @return {!CrOnc.NetworkProperties}
   * @private
   */
  getCellularState_: function(deviceState) {
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
   * @param {!DeviceStateProperties|undefined} deviceState
   * @return {boolean} Whether or not the device state is enabled.
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return !!deviceState &&
        deviceState.State == chrome.networkingPrivate.DeviceStateType.ENABLED;
  },

  /**
   * @param {!DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsVisible_: function(deviceState) {
    return deviceState.Type != CrOnc.Type.ETHERNET &&
        deviceState.Type != CrOnc.Type.VPN;
  },

  /**
   * @param {!DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  enableToggleIsEnabled_: function(deviceState) {
    return deviceState.State !=
        chrome.networkingPrivate.DeviceStateType.PROHIBITED;
  },

  /**
   * @param {!DeviceStateProperties} deviceState
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
   * @return {boolean}
   * @private
   */
  showDetailsIsVisible_: function() {
    return this.deviceIsEnabled_(this.deviceState) &&
        (!!this.activeNetworkState.GUID || this.networkStateList.length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowList_: function() {
    var minlen = (this.deviceState.Type == CrOnc.Type.WI_FI ||
                  this.deviceState.Type == CrOnc.Type.VPN ||
                  this.deviceState.Type == CrOnc.Type.TETHER) ?
        1 :
        2;
    return this.networkStateList.length >= minlen;
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
    } else if (this.shouldShowList_()) {
      this.fire('show-networks', this.deviceState);
    } else if (this.activeNetworkState.GUID) {
      this.fire('show-detail', this.activeNetworkState);
    } else if (this.networkStateList.length > 0) {
      this.fire('show-detail', this.networkStateList[0]);
    }
    event.stopPropagation();
  },

  /**
   * @return {string}
   * @private
   */
  getDetailsA11yString_: function() {
    if (!this.shouldShowList_()) {
      if (this.activeNetworkState.GUID) {
        return CrOnc.getNetworkName(this.activeNetworkState);
      } else if (this.networkStateList.length > 0) {
        return CrOnc.getNetworkName(this.networkStateList[0]);
      }
    }
    return this.i18n('OncType' + this.deviceState.Type);
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
