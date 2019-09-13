// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-detail-dialog' is used in the login screen to show a subset of
 * internet details and allow configuration of proxy, IP, and nameservers.
 */
Polymer({
  is: 'internet-detail-dialog',

  behaviors: [
    CrNetworkListenerBehavior,
    CrPolicyNetworkBehavior,
    I18nBehavior,
  ],

  properties: {
    /** The network GUID to display details for. */
    guid: String,

    /**
     * The current properties for the network matching |guid|.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: Object,

    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /**
     * Whether to show technology badge on mobile network icons.
     * @private
     */
    showTechnologyBadge_: {
      type: Boolean,
      value: function() {
        return loadTimeData.valueExists('showTechnologyBadge') &&
            loadTimeData.getBoolean('showTechnologyBadge');
      }
    },
  },

  /**
   * Set to true to once the initial properties have been received. This
   * prevents setProperties from being called when setting default properties.
   * @private {boolean}
   */
  networkPropertiesReceived_: false,

  /** @override */
  attached: function() {
    var dialogArgs = chrome.getVariableValue('dialogArguments');
    var type, name;
    if (dialogArgs) {
      var args = JSON.parse(dialogArgs);
      this.guid = args.guid || '';
      type = args.type || 'WiFi';
      name = args.name || type;
    } else {
      // For debugging
      var params = new URLSearchParams(document.location.search.substring(1));
      this.guid = params.get('guid') || '';
      type = params.get('type') || 'WiFi';
      name = params.get('name') || type;
    }

    if (!this.guid) {
      console.error('Invalid guid');
      this.close_();
    }

    // Set basic networkProperties until they are loaded.
    this.networkProperties = {
      GUID: this.guid,
      Type: type,
      ConnectionState: CrOnc.ConnectionState.NOT_CONNECTED,
      Name: {Active: name},
    };
    this.networkPropertiesReceived_ = false;
    this.getNetworkDetails_();
  },

  /** @override */
  ready: function() {
    CrOncStrings = {
      OncTypeCellular: loadTimeData.getString('OncTypeCellular'),
      OncTypeEthernet: loadTimeData.getString('OncTypeEthernet'),
      OncTypeMobile: loadTimeData.getString('OncTypeMobile'),
      OncTypeTether: loadTimeData.getString('OncTypeTether'),
      OncTypeVPN: loadTimeData.getString('OncTypeVPN'),
      OncTypeWiFi: loadTimeData.getString('OncTypeWiFi'),
      OncTypeWiMAX: loadTimeData.getString('OncTypeWiMAX'),
      networkListItemConnected:
          loadTimeData.getString('networkListItemConnected'),
      networkListItemConnecting:
          loadTimeData.getString('networkListItemConnecting'),
      networkListItemConnectingTo:
          loadTimeData.getString('networkListItemConnectingTo'),
      networkListItemInitializing:
          loadTimeData.getString('networkListItemInitializing'),
      networkListItemScanning:
          loadTimeData.getString('networkListItemScanning'),
      networkListItemNotConnected:
          loadTimeData.getString('networkListItemNotConnected'),
      networkListItemNoNetwork:
          loadTimeData.getString('networkListItemNoNetwork'),
      vpnNameTemplate: loadTimeData.getString('vpnNameTemplate'),
    };
  },

  /** @private */
  close_: function() {
    chrome.send('dialogClose');
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<OncMojo.NetworkStateProperties>} networks
   */
  onActiveNetworksChanged: function(networks) {
    if (!this.guid || !this.networkProperties) {
      return;
    }
    // If the network was or is active, request an update.
    if (this.networkProperties.ConnectionState !=
            CrOnc.ConnectionState.NOT_CONNECTED ||
        networks.find(network => network.guid == this.guid)) {
      this.getNetworkDetails_();
    }
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   */
  onDeviceStateListChanged: function() {
    if (!this.guid || !this.networkProperties) {
      return;
    }
    this.getNetworkDetails_();
  },

  /**
   * Calls networkingPrivate.getProperties for this.guid.
   * @private
   */
  getNetworkDetails_: function() {
    assert(this.guid);
    this.networkingPrivate.getManagedProperties(
        this.guid, this.getPropertiesCallback_.bind(this));
  },

  /**
   * networkingPrivate.getProperties callback.
   * @param {!CrOnc.NetworkProperties} properties The network properties.
   * @private
   */
  getPropertiesCallback_: function(properties) {
    if (chrome.runtime.lastError) {
      var message = chrome.runtime.lastError.message;
      if (message == 'Error.InvalidNetworkGuid') {
        console.error('Details page: GUID no longer exists: ' + this.guid);
      } else {
        console.error(
            'Unexpected networkingPrivate.getManagedProperties error: ' +
            message + ' For: ' + this.guid);
      }
      this.close_();
      return;
    }
    if (!properties) {
      console.error('No properties for: ' + this.guid);
      this.close_();
      return;
    }
    this.networkProperties = properties;
    this.networkPropertiesReceived_ = true;
  },

  /**
   * @param {!CrOnc.NetworkProperties} properties
   * @return {!OncMojo.NetworkStateProperties}
   */
  getNetworkState_: function(properties) {
    return OncMojo.oncPropertiesToNetworkState(properties);
  },

  /**
   * @param {!chrome.networkingPrivate.NetworkConfigProperties} onc The ONC
   *     network properties.
   * @private
   */
  setNetworkProperties_: function(onc) {
    if (!this.networkPropertiesReceived_)
      return;

    assert(this.guid);
    this.networkingPrivate.setProperties(this.guid, onc, () => {
      if (chrome.runtime.lastError) {
        // An error typically indicates invalid input; request the properties
        // to update any invalid fields.
        this.getNetworkDetails_();
      }
    });
  },

  /**
   * @return {!chrome.networkingPrivate.NetworkConfigProperties} An ONC
   *     dictionary with just the Type property set. Used for passing properties
   *     to setNetworkProperties_.
   * @private
   */
  getEmptyNetworkProperties_: function() {
    return {Type: this.networkProperties.Type};
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {string}
   * @private
   */
  getStateText_: function(networkProperties) {
    if (!networkProperties.ConnectionState)
      return '';
    return this.i18n('Onc' + networkProperties.ConnectionState);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {string}
   * @private
   */
  getNameText_: function(networkProperties) {
    return CrOnc.getNetworkName(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean} True if the network is connected.
   * @private
   */
  isConnectedState_: function(networkProperties) {
    return networkProperties.ConnectionState == CrOnc.ConnectionState.CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isRemembered_: function(networkProperties) {
    var source = networkProperties.Source;
    return !!source && source != CrOnc.Source.NONE;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isRememberedOrConnected_: function(networkProperties) {
    return this.isRemembered_(networkProperties) ||
        this.isConnectedState_(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  isCellular_: function(networkProperties) {
    return networkProperties.Type == CrOnc.Type.CELLULAR &&
        !!networkProperties.Cellular;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showCellularSim_: function(networkProperties) {
    return networkProperties.Type == CrOnc.Type.CELLULAR &&
        !!networkProperties.Cellular &&
        networkProperties.Cellular.Family != 'CDMA';
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showCellularChooseNetwork_: function(networkProperties) {
    return networkProperties.Type == CrOnc.Type.CELLULAR &&
        !!this.get('Cellular.SupportNetworkScan', this.networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {string}
   * @private
   */
  getConnectDisconnectText_: function(networkProperties) {
    if (this.showConnect_(networkProperties)) {
      return this.i18n('networkButtonConnect');
    }
    return this.i18n('networkButtonDisconnect');
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showConnectDisconnect_: function(networkProperties) {
    return this.showConnect_(networkProperties) ||
        this.showDisconnect_(networkProperties);
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showConnect_: function(networkProperties) {
    return networkProperties.Type != CrOnc.Type.ETHERNET &&
        networkProperties.ConnectionState ==
        CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  showDisconnect_: function(networkProperties) {
    return networkProperties.Type != CrOnc.Type.ETHERNET &&
        networkProperties.ConnectionState !=
        CrOnc.ConnectionState.NOT_CONNECTED;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  shouldShowProxyPolicyIndicator_: function(networkProperties) {
    var property = this.get('ProxySettings.Type', networkProperties);
    return !!property &&
        this.isNetworkPolicyEnforced(
            /** @type {!CrOnc.ManagedProperty} */ (property));
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean}
   * @private
   */
  enableConnectDisconnect_: function(networkProperties) {
    if (!this.showConnectDisconnect_(networkProperties)) {
      return false;
    }

    if (this.showConnect_(networkProperties)) {
      return this.enableConnect_(networkProperties);
    }

    return true;
  },

  /**
   * @param {!CrOnc.NetworkProperties} networkProperties
   * @return {boolean} Whether or not to enable the network connect button.
   * @private
   */
  enableConnect_: function(networkProperties) {
    if (!this.showConnect_(networkProperties))
      return false;
    if (networkProperties.Type == CrOnc.Type.CELLULAR &&
        CrOnc.isSimLocked(networkProperties)) {
      return false;
    }
    return true;
  },

  /** @private */
  onConnectDisconnectClick_: function() {
    assert(this.networkProperties);
    if (!this.showConnect_(this.networkProperties)) {
      this.networkingPrivate.startDisconnect(this.guid);
      return;
    }

    var properties = this.networkProperties;
    this.networkingPrivate.startConnect(properties.GUID, function() {
      if (chrome.runtime.lastError) {
        var message = chrome.runtime.lastError.message;
        if (message == 'connecting' || message == 'connect-canceled' ||
            message == 'connected' || message == 'Error.InvalidNetworkGuid') {
          return;
        }
        console.error(
            'Unexpected networkingPrivate.startConnect error: ' + message +
            ' For: ' + properties.GUID);
      }
    });
  },

  /**
   * Event triggered for elements associated with network properties.
   * @param {!CustomEvent<!{field: string, value: (string|!Object)}>} event
   * @private
   */
  onNetworkPropertyChange_: function(event) {
    if (!this.networkProperties)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    var onc = this.getEmptyNetworkProperties_();
    if (field == 'APN') {
      CrOnc.setTypeProperty(onc, 'APN', value);
    } else if (field == 'SIMLockStatus') {
      CrOnc.setTypeProperty(onc, 'SIMLockStatus', value);
    } else {
      console.error('Unexpected property change event: ' + field);
      return;
    }
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the IP Config or NameServers element changes.
   * TODO(stevenjb): Move this logic down to network_ip_config.js and
   * network_nameservers.js and remove it from here and internet_detail_page.js.
   * @param {!CustomEvent<!{
   *     field: string,
   *     value: (string|!CrOnc.IPConfigProperties|!Array<string>)
   * }>} event The network-ip-config or network-nameservers change event.
   * @private
   */
  onIPConfigChange_: function(event) {
    if (!this.networkProperties)
      return;
    var field = event.detail.field;
    var value = event.detail.value;
    // Get an empty ONC dictionary and set just the IP Config properties that
    // need to change.
    var onc = this.getEmptyNetworkProperties_();
    var ipConfigType =
        /** @type {chrome.networkingPrivate.IPConfigType|undefined} */ (
            CrOnc.getActiveValue(this.networkProperties.IPAddressConfigType));
    if (field == 'IPAddressConfigType') {
      var newIpConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */ (value);
      if (newIpConfigType == ipConfigType)
        return;
      onc.IPAddressConfigType = newIpConfigType;
    } else if (field == 'NameServersConfigType') {
      var nsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType|undefined} */ (
              CrOnc.getActiveValue(
                  this.networkProperties.NameServersConfigType));
      var newNsConfigType =
          /** @type {chrome.networkingPrivate.IPConfigType} */ (value);
      if (newNsConfigType == nsConfigType)
        return;
      onc.NameServersConfigType = newNsConfigType;
    } else if (field == 'StaticIPConfig') {
      if (ipConfigType == CrOnc.IPConfigType.STATIC) {
        var staticIpConfig = this.networkProperties.StaticIPConfig;
        var ipConfigValue = /** @type {!Object} */ (value);
        if (staticIpConfig &&
            this.allPropertiesMatch_(staticIpConfig, ipConfigValue)) {
          return;
        }
      }
      onc.IPAddressConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */ ({});
      }
      // Only copy Static IP properties.
      var keysToCopy = ['Type', 'IPAddress', 'RoutingPrefix', 'Gateway'];
      for (var i = 0; i < keysToCopy.length; ++i) {
        var key = keysToCopy[i];
        if (key in value)
          onc.StaticIPConfig[key] = value[key];
      }
    } else if (field == 'NameServers') {
      // If a StaticIPConfig property is specified and its NameServers value
      // matches the new value, no need to set anything.
      var nameServers = /** @type {!Array<string>} */ (value);
      if (onc.NameServersConfigType == CrOnc.IPConfigType.STATIC &&
          onc.StaticIPConfig && onc.StaticIPConfig.NameServers == nameServers) {
        return;
      }
      onc.NameServersConfigType = CrOnc.IPConfigType.STATIC;
      if (!onc.StaticIPConfig) {
        onc.StaticIPConfig =
            /** @type {!chrome.networkingPrivate.IPConfigProperties} */ ({});
      }
      onc.StaticIPConfig.NameServers = nameServers;
    } else {
      console.error('Unexpected change field: ' + field);
      return;
    }
    // setValidStaticIPConfig will fill in any other properties from
    // networkProperties. This is necessary since we update IP Address and
    // NameServers independently.
    CrOnc.setValidStaticIPConfig(onc, this.networkProperties);
    this.setNetworkProperties_(onc);
  },

  /**
   * Event triggered when the Proxy configuration element changes.
   * @param {!CustomEvent<!{field: string, value: !CrOnc.ProxySettings}>} event
   *     The network-proxy change event.
   * @private
   */
  onProxyChange_: function(event) {
    if (!this.networkProperties)
      return;
    if (event.detail.field != 'ProxySettings')
      return;
    var onc = this.getEmptyNetworkProperties_();
    CrOnc.setProperty(
        onc, 'ProxySettings', /** @type {!Object} */ (event.detail.value));
    this.setNetworkProperties_(onc);
  },

  /**
   * @param {!Array<string>} fields
   * @return {boolean}
   * @private
   */
  hasVisibleFields_: function(fields) {
    return fields.some((field) => {
      var value = this.get(field, this.networkProperties);
      return value !== undefined && value !== '';
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  hasInfoFields_: function() {
    return this.hasVisibleFields_(this.getInfoFields_());
  },

  /**
   * @return {!Array<string>} The fields to display in the info section.
   * @private
   */
  getInfoFields_: function() {
    /** @type {!Array<string>} */ var fields = [];
    var type = this.networkProperties.Type;
    if (type == CrOnc.Type.CELLULAR && this.networkProperties.Cellular) {
      fields.push(
          'Cellular.HomeProvider.Name', 'Cellular.ServingOperator.Name',
          'Cellular.ActivationState', 'Cellular.RoamingState',
          'RestrictedConnectivity', 'Cellular.MEID', 'Cellular.ESN',
          'Cellular.ICCID', 'Cellular.IMEI', 'Cellular.IMSI', 'Cellular.MDN',
          'Cellular.MIN');
    } else if (type == CrOnc.Type.WI_FI) {
      fields.push('RestrictedConnectivity');
    } else if (type == CrOnc.Type.WI_MAX) {
      fields.push('RestrictedConnectivity', 'WiMAX.EAP.Identity');
    }
    fields.push('MacAddress');
    return fields;
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
    return Object.getOwnPropertyNames(newValue).every(
        key => newValue[key] == curValue[key]);
  }
});
