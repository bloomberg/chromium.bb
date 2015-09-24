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
'use strict';

/** @const */ var CARRIER_VERIZON = 'Verizon Wireless';

Polymer({
  is: 'cr-settings-internet-detail-page',

  properties: {
    /**
     * The network GUID to display details for.
     */
    guid: {
      type: String,
      value: '',
      observer: 'guidChanged_',
    },

    /**
     * The current state for the network matching |guid|. TODO(stevenjb): Use
     * chrome.networkingProperties.NetworkPoperties once it is defined. This
     * will be a super-set of NetworkStateProperties. Currently properties that
     * are not defined in NetworkStateProperties are accessed through
     * CrOnc.getActive* which uses [] to access the property, which avoids any
     * type checking (see CrOnc.getProperty for more info).
     * @type {CrOnc.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
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

    /**
     * The network preferred state.
     */
    preferNetwork: {
      type: Boolean,
      value: false,
      observer: 'preferNetworkChanged_'
    },

    /**
     * The network IP Address.
     */
    IPAddress: {
      type: String,
      value: ''
    },

    /**
     * Object providing network type values for data binding.
     * @const
     */
    NetworkType: {
      type: Object,
      value: {
        CELLULAR: CrOnc.Type.CELLULAR,
        ETHERNET: CrOnc.Type.ETHERNET,
        VPN: CrOnc.Type.VPN,
        WIFI: CrOnc.Type.WI_FI,
        WIMAX: CrOnc.Type.WI_MAX,
      },
      readOnly: true
    },
  },

  /**
   * Listener function for chrome.networkingPrivate.onNetworksChanged event.
   * @type {function(!Array<string>)}
   * @private
   */
  networksChangedListener_: function() {},

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
   * Polymer networkState changed method.
   */
  networkStateChanged_: function() {
    if (!this.networkState)
      return;

    // Update autoConnect if it has changed. Default value is false.
    var autoConnect = /** @type {boolean} */(
        CrOnc.getActiveTypeValue(this.networkState, 'AutoConnect')) || false;
    if (autoConnect != this.autoConnect)
      this.autoConnect = autoConnect;

    // Update preferNetwork if it has changed. Default value is false.
    var preferNetwork = this.networkState.Priority > 0;
    if (preferNetwork != this.preferNetwork)
      this.preferNetwork = preferNetwork;

    // Set the IPAddress property to the IPV4 Address.
    var ipv4 = CrOnc.getIPConfigForType(this.networkState, CrOnc.IPType.IPV4);
    this.IPAddress = (ipv4 && ipv4.IPAddress) || '';
  },

  /**
   * Polymer autoConnect changed method.
   */
  autoConnectChanged_: function() {
    if (!this.networkState || !this.guid)
      return;
    var onc = this.getEmptyNetworkProperties_();
    CrOnc.setTypeProperty(onc, 'AutoConnect', this.autoConnect);
    this.setNetworkProperties_(onc);
  },

  /**
   * Polymer preferNetwork changed method.
   */
  preferNetworkChanged_: function() {
    if (!this.networkState || !this.guid)
      return;
    var onc = this.getEmptyNetworkProperties_();
    onc.Priority = this.preferNetwork ? 1 : 0;
    this.setNetworkProperties_(onc);
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
   * Calls networkingPrivate.getProperties for this.guid.
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
   * @param {Object} properties The network properties.
   * @private
   */
  getPropertiesCallback_: function(properties) {
    this.networkState = /** @type {CrOnc.NetworkStateProperties}*/(properties);
    if (!properties) {
      // If state becomes null (i.e. the network is no longer visible), close
      // the page.
      this.fire('close');
    }
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} onc The ONC
   *     network properties.
   * @private
   */
  setNetworkProperties_: function(onc) {
    if (!this.guid)
      return;
    chrome.networkingPrivate.setProperties(this.guid, onc, function() {
      if (chrome.runtime.lastError) {
        // An error typically indicates invalid input; request the properties
        // to update any invalid fields.
        this.getNetworkDetails_();
      }
    }.bind(this));
  },

  /**
   * @return {!chrome.networkingPrivate.NetworkConfigProperties} An ONC
   *     dictionary with just the Type property set. Used for passing properties
   *     to setNetworkProperties_.
   * @private
   */
  getEmptyNetworkProperties_: function() {
    return {Type: this.networkState.Type};
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
    return !!state && state.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} Whether or not to show the 'Connect' button.
   * @private
   */
  showConnect_: function(state) {
    return !!state && state.Type != CrOnc.Type.ETHERNET &&
           state.ConnectionState == CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} Whether or not to show the 'Activate' button.
   * @private
   */
  showActivate_: function(state) {
    if (!state || state.Type != CrOnc.Type.CELLULAR)
      return false;
    var activation = state.Cellular.ActivationState;
    return activation == CrOnc.ActivationState.NOT_ACTIVATED ||
           activation == CrOnc.ActivationState.PARTIALLY_ACTIVATED;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} Whether or not to show the 'View Account' button.
   * @private
   */
  showViewAccount_: function(state) {
    // Show either the 'Activate' or the 'View Account' button.
    if (this.showActivate_(state))
      return false;

    if (!state || state.Type != CrOnc.Type.CELLULAR || !state.Cellular)
      return false;

    // Only show if online payment URL is provided or the carrier is Verizon.
    var carrier = CrOnc.getActiveValue(state, 'Cellular.Carrier');
    if (carrier != CARRIER_VERIZON) {
      var paymentPortal = /** @type {CrOnc.PaymentPortal|undefined} */(
          this.get('state.Cellular.PaymentPortal'));
      if (!paymentPortal || !paymentPortal.Url)
        return false;
    }

    // Only show for connected networks or LTE networks with a valid MDN.
    if (!this.isConnectedState_(state)) {
      var technology = /** @type {CrOnc.NetworkTechnology|undefined} */(
          CrOnc.getActiveValue(state, 'Cellular.NetworkTechnology'));
      if (technology != CrOnc.NetworkTechnology.LTE &&
          technology != CrOnc.NetworkTechnology.LTE_ADVANCED) {
        return false;
      }
      if (!CrOnc.getActiveValue(state, 'Cellular.MDN'))
        return false;
    }

    return true;
  },

  /**
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(state) {
    if (!state || !this.showConnect_(state))
      return false;
    if (state.Type == CrOnc.Type.CELLULAR && CrOnc.isSimLocked(state))
      return false;
    // TODO(stevenjb): For VPN, check connected state of any network.
    return true;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} Whether or not to show the 'Disconnect' button.
   * @private
   */
  showDisconnect_: function(state) {
    return !!state && state.Type != CrOnc.Type.ETHERNET &&
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
   * Callback when the Activate button is clicked.
   * @private
   */
  onActivateClicked_: function() {
    chrome.networkingPrivate.startActivate(this.guid);
  },

  /**
   * Callback when the View Account button is clicked.
   * @private
   */
  onViewAccountClicked_: function() {
    // startActivate() will show the account page for activated networks.
    chrome.networkingPrivate.startActivate(this.guid);
  },

  /**
   * Event triggered for elements associated with network properties.
   * @param {!{detail: !{field: string, value: (string|!Object)}}} event
   * @private
   */
  onNetworkPropertyChange_: function(event) {
    if (!this.networkState)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    var onc = this.getEmptyNetworkProperties_();
    if (field == 'APN') {
      CrOnc.setTypeProperty(onc, 'APN', value);
    } else if (field == 'SIMLockStatus') {
      CrOnc.setTypeProperty(onc, 'SIMLockStatus', value);
    } else {
      console.error('Unexpected property change event: ', field);
      return;
    }
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the IP Config or NameServers element changes.
   * @param {!{detail: !{field: string,
   *                     value: (string|!CrOnc.IPConfigProperties|
   *                             !Array<string>)}}} event
   *     The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_: function(event) {
    if (!this.networkState)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    // Get an empty ONC dictionary and set just the IP Config properties that
    // need to change.
    var onc = this.getEmptyNetworkProperties_();
    var ipConfigType =
        /** @type {chrome.networkingPrivate.IPConfigType|undefined} */(
            CrOnc.getActiveValue(this.networkState, 'IPAddressConfigType'));
    if (field == 'IPAddressConfigType') {
      var newIpConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */(value);
      if (newIpConfigType == ipConfigType)
        return;
      onc.IPAddressConfigType = newIpConfigType;
    } else if (field == 'NameServersConfigType') {
      var nsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType|undefined} */(
              CrOnc.getActiveValue(this.networkState, 'NameServersConfigType'));
      var newNsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */(value);
      if (newNsConfigType == nsConfigType)
        return;
      onc.NameServersConfigType = newNsConfigType;
    } else if (field == 'StaticIPConfig') {
      if (ipConfigType == CrOnc.IPConfigType.STATIC) {
        var staticIpConfig = /** @type {CrOnc.IPConfigProperties|undefined} */(
            this.get('networkState.StaticIPConfig'));
        if (staticIpConfig &&
            this.allPropertiesMatch_(staticIpConfig,
                                     /** @type {!Object} */(value))) {
          return;
        }
      }
      onc.IPAddressConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */({});
      }
      for (let key in value)
        onc.StaticIPConfig[key] = value[key];
    } else if (field == 'NameServers') {
      // If a StaticIPConfig property is specified and its NameServers value
      // matches the new value, no need to set anything.
      var nameServers = /** @type {!Array<string>} */(value);
      if (onc.NameServersConfigType == CrOnc.IPConfigType.STATIC &&
          onc.StaticIPConfig &&
          onc.StaticIPConfig.NameServers == nameServers) {
        return;
      }
      onc.NameServersConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */({});
      }
      onc.StaticIPConfig.NameServers = nameServers;
    } else {
      console.error('Unexpected change field: ' + field);
      return;
    }
    // setValidStaticIPConfig will fill in any other properties from
    // networkState. This is necessary since we update IP Address and
    // NameServers independently.
    CrOnc.setValidStaticIPConfig(onc, this.networkState);
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the Proxy configuration element changes.
   * @param {!{detail: {field: string, value: !CrOnc.ProxySettings}}} event
   *     The network-proxy change event.
   * @private
   */
  onProxyChange_: function(event) {
    if (!this.networkState)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    if (field != 'ProxySettings')
      return;
    var onc = this.getEmptyNetworkProperties_();
    CrOnc.setProperty(onc, 'ProxySettings', /** @type {!Object} */(value));
    this.setNetworkProperties_(onc);
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if the shared message should be shown.
   * @private
   */
  showShared_: function(state) {
    return !!state &&
           (state.Source == 'Device' || state.Source == 'DevicePolicy');
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if the AutoConnect checkbox should be shown.
   * @private
   */
  showAutoConnect_: function(state) {
    return !!state && state.Type != CrOnc.Type.ETHERNET &&
           state.Source != CrOnc.Source.NONE;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if the prefer network checkbox should be shown.
   * @private
   */
  showPreferNetwork_: function(state) {
    // TODO(stevenjb): Resolve whether or not we want to allow "preferred" for
    // state.Type == CrOnc.Type.ETHERNET.
    return !!state && state.Source != CrOnc.Source.NONE;
  },

  /**
   * @param {boolean} preferNetwork
   * @return {string} The icon to use for the preferred button.
   * @private
   */
  getPreferredIcon_: function(preferNetwork) {
    return preferNetwork ? 'star' : 'star-border';
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

    if (state.Type == CrOnc.Type.CELLULAR) {
      fields.push('Cellular.ActivationState',
                  'Cellular.RoamingState',
                  'RestrictedConnectivity',
                  'Cellular.ServingOperator.Name');
    }
    if (state.Type == CrOnc.Type.VPN) {
      fields.push('VPN.Host', 'VPN.Type');
      if (state.VPN.Type == 'OpenVPN')
        fields.push('VPN.OpenVPN.Username');
      else if (state.VPN.Type == 'L2TP-IPsec')
        fields.push('VPN.L2TP.Username');
      else if (state.VPN.Type == 'ThirdPartyVPN')
        fields.push('VPN.ThirdPartyVPN.ProviderName');
    }
    if (state.Type == CrOnc.Type.WI_FI)
      fields.push('RestrictedConnectivity');
    if (state.Type == CrOnc.Type.WI_MAX) {
      fields.push('RestrictedConnectivity', 'WiMAX.EAP.Identity');
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
    if (state.Type == CrOnc.Type.CELLULAR) {
      fields.push('Cellular.Carrier',
                  'Cellular.Family',
                  'Cellular.NetworkTechnology',
                  'Cellular.ServingOperator.Code');
    }
    if (state.Type == CrOnc.Type.WI_FI) {
      fields.push('WiFi.SSID',
                  'WiFi.BSSID',
                  'WiFi.Security',
                  'WiFi.SignalStrength',
                  'WiFi.Frequency');
    }
    if (state.Type == CrOnc.Type.WI_MAX)
      fields.push('WiFi.SignalStrength');
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
    if (state.Type == CrOnc.Type.CELLULAR) {
      fields.push('Cellular.HomeProvider.Name',
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
                  'Cellular.PRLVersion');
    }
    return fields;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if there are any advanced fields to display.
   * @private
   */
  hasAdvancedOrDeviceFields_: function(state) {
    return this.getAdvancedFields_(state).length > 0 ||
           this.hasDeviceFields_(state);
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if there are any device fields to display.
   * @private
   */
  hasDeviceFields_: function(state) {
    var fields = this.getDeviceFields_(state);
    return fields.length > 0;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if the network section should be shown.
   * @private
   */
  hasNetworkSection_: function(state) {
    return !!state && state.Type != CrOnc.Type.VPN;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @param {string} type The network type.
   * @return {boolean} True if the network type matches 'type'.
   * @private
   */
  isType_: function(state, type) {
    return !!state && state.Type == type;
  },

  /**
   * @param {?CrOnc.NetworkStateProperties} state The network state properties.
   * @return {boolean} True if the Cellular SIM section should be shown.
   * @private
   */
  showCellularSim_: function(state) {
    if (!state || state.Type != 'Cellular')
      return false;
    return CrOnc.getActiveValue(state, 'Cellular.Family') == 'GSM';
  },

  /**
   * @param {!Object} curValue
   * @param {!Object} newValue
   * @return {boolean} True if all properties set in |newValue| are equal to
   *     the corresponding properties in |curValue|. Note: Not all properties
   *     of |curValue| need to be specified in |newValue| for this to return
   *     true.
   * @private
   */
  allPropertiesMatch_: function(curValue, newValue) {
    for (let key in newValue) {
      if (newValue[key] != curValue[key])
        return false;
    }
    return true;
  }
});
})();
