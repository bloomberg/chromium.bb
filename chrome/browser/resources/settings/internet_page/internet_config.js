// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-config' provides configuration of authentication
 * properties for new and existing networks.
 */
Polymer({
  is: 'settings-internet-config',

  behaviors: [settings.RouteObserverBehavior, I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /**
     * The GUID when an existing network is being configured. This will be
     * empty when configuring a new network.
     * @private
     */
    guid_: String,

    /**
     * The current properties if an existing network being configured.
     * This will be undefined when configuring a new network.
     * @private {!chrome.networkingPrivate.NetworkProperties|undefined}
     */
    networkProperties_: Object,

    /** Set if |guid_| is not empty once networkProperties_ are received. */
    propertiesReceived_: Boolean,

    /** Set once properties have been sent; prevents multiple saves. */
    propertiesSent_: Boolean,

    /**
     * The configuration properties for the network. |configProperties_.Type|
     * will always be defined as the network type being configured.
     * @private {!chrome.networkingPrivate.NetworkConfigProperties}
     */
    configProperties_: Object,

    /**
     * The title to display (network name or type).
     * @private
     */
    title_: {
      type: String,
      computed: 'computeTitle_(networkProperties_)',
    },

    /**
     * Whether this network should be shared with other users of the device.
     * @private
     */
    shareNetwork_: {
      type: Boolean,
      value: true,
    },

    /**
     * Saved security value, used to detect when Security changes.
     * @private
     */
    savedSecurity_: String,

    /**
     * Dictionary of boolean values determining which EAP properties to show,
     * or null to hide all EAP settings.
     * @type {?{
     *   Inner: boolean,
     *   ServerCA: boolean,
     *   SubjectMatch: boolean,
     *   UserCert: boolean,
     *   Password: boolean,
     *   AnonymousIdentity: boolean,
     * }}
     * @private
     */
    showEap_: {
      type: Object,
      value: null,
    },

    /**
     * Object providing network type values for data binding. Note: Currently
     * we only support WiFi, but support for other types will be following
     * shortly.
     * @const
     * @private
     */
    NetworkType_: {
      type: Object,
      value: {
        ETHERNET: CrOnc.Type.ETHERNET,
        VPN: CrOnc.Type.VPN,
        WI_FI: CrOnc.Type.WI_FI,
        WI_MAX: CrOnc.Type.WI_MAX,
      },
      readOnly: true
    },

    /**
     * Array of values for the WiFi Security dropdown.
     * @type {!Array<string>}
     * @const
     * @private
     */
    securityItems_: {
      type: Array,
      readOnly: true,
      value: [
        CrOnc.Security.NONE, CrOnc.Security.WEP_PSK, CrOnc.Security.WPA_PSK,
        CrOnc.Security.WPA_EAP
      ],
    },

    /**
     * Array of values for the EAP Method (Outer) dropdown.
     * @type {!Array<string>}
     * @const
     * @private
     */
    eapOuterItems_: {
      type: Array,
      readOnly: true,
      value: [
        CrOnc.EAPType.LEAP, CrOnc.EAPType.PEAP, CrOnc.EAPType.EAP_TLS,
        CrOnc.EAPType.EAP_TTLS
      ],
    },

    /**
     * Array of values for the EAP EAP Phase 2 authentication (Inner) dropdown
     * when the Outer type is PEAP.
     * @type {!Array<string>}
     * @const
     * @private
     */
    eapInnerItemsPeap_: {
      type: Array,
      readOnly: true,
      value: ['Automatic', 'MD5', 'MSCHAPv2'],
    },

    /**
     * Array of values for the EAP EAP Phase 2 authentication (Inner) dropdown
     * when the Outer type is EAP-TTLS.
     * @type {!Array<string>}
     * @const
     * @private
     */
    eapInnerItemsTtls_: {
      type: Array,
      readOnly: true,
      value: ['Automatic', 'MD5', 'MSCHAP', 'MSCHAPv2', 'PAP', 'CHAP', 'GTC'],
    },
  },

  observers: [
    'updateConfigProperties_(networkProperties_)',
    'updateWiFiSecurity_(configProperties_.WiFi.Security)',
    'updateWiFiEapOuter_(configProperties_.WiFi.EAP.Outer)',
  ],

  /** @const */
  MIN_PASSPHRASE_LENGTH: 5,

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.Route.NETWORK_CONFIG)
      return;

    var queryParams = settings.getQueryParameters();
    this.guid_ = queryParams.get('guid') || '';

    // Set networkProperties for new configurations and for existing
    // configurations until the current properties are loaded.
    var name = queryParams.get('name') || '';
    var typeParam = queryParams.get('type');
    var type = typeParam ? CrOnc.getValidType(typeParam) : CrOnc.Type.WI_FI;
    assert(type && type != CrOnc.Type.ALL);
    this.networkProperties_ = {
      GUID: this.guid_,
      Name: name,
      Type: type,
    };
    if (this.guid_) {
      this.networkingPrivate.getProperties(
          this.guid_, this.getPropertiesCallback_.bind(this));
    }
  },

  /** @private */
  close_: function() {
    if (settings.getCurrentRoute() == settings.Route.NETWORK_CONFIG)
      settings.navigateToPreviousRoute();
  },

  /**
   * networkingPrivate.getProperties callback.
   * @param {!chrome.networkingPrivate.NetworkProperties} properties
   * @private
   */
  getPropertiesCallback_: function(properties) {
    if (!properties) {
      // If |properties| is null, the network no longer exists; close the page.
      console.error('Network no longer exists: ' + this.guid_);
      this.close_();
      return;
    }
    this.propertiesReceived_ = true;
    this.networkProperties_ = properties;

    // Set the current shareNetwork_ value when porperties are received.
    var source = this.networkProperties_.Source;
    this.shareNetwork_ = source == CrOnc.Source.DEVICE ||
        source == CrOnc.Source.DEVICE_POLICY;
  },

  /**
   * @return {string}
   * @private
   */
  computeTitle_: function() {
    return this.networkProperties_.Name ||
        this.i18n('OncType' + this.networkProperties_.Type);
  },

  /**
   * Updates the config properties when |this.networkProperties_| changes.
   * This gets called once when navigating to the page when default properties
   * are set, and again for existing networks when the properties are received.
   * @private
   */
  updateConfigProperties_: function() {
    var properties = this.networkProperties_;
    var configProperties =
        /** @type {chrome.networkingPrivate.NetworkConfigProperties} */ ({
          Name: properties.Name || '',
          Type: properties.Type,
        });
    if (properties.Type == CrOnc.Type.WI_FI) {
      if (properties.WiFi) {
        configProperties.WiFi = {
          AutoConnect: properties.WiFi.AutoConnect,
          EAP: properties.WiFi.EAP,
          Passphrase: properties.WiFi.Passphrase,
          SSID: properties.WiFi.SSID,
          Security: properties.WiFi.Security
        };
        this.savedSecurity_ = properties.WiFi.Security || '';
      } else {
        configProperties.WiFi = {
          AutoConnect: false,
          SSID: '',
          Security: CrOnc.Security.NONE,
        };
      }
    }
    this.configProperties_ = configProperties;
  },

  /**
   * Ensures that the appropriate properties are set or deleted when the
   * Security type changes.
   * @private
   */
  updateWiFiSecurity_: function() {
    assert(this.configProperties_.WiFi);
    var security = this.configProperties_.WiFi.Security || CrOnc.Security.NONE;
    if (security == this.savedSecurity_)
      return;
    this.savedSecurity_ = security || '';

    if (!this.guid_) {
      // Set the default share state for new configurations.
      // TODO(stevenjb): also check login state.
      this.shareNetwork_ = security == CrOnc.Security.NONE;
    }

    if (security == CrOnc.Security.WPA_EAP)
      this.set('configProperties_.WiFi.EAP', {Outer: CrOnc.EAPType.LEAP});
    else
      delete this.configProperties_.WiFi.EAP;
  },

  /**
   * Ensures that the appropriate EAP properties are created (or deleted when
   * the EAP.Outer property changes.
   * @private
   */
  updateWiFiEapOuter_: function() {
    var eap = this.configProperties_.WiFi.EAP;
    if (!eap || !eap.Outer)
      return;
    var innerItems = this.getEapInnerItems_(eap.Outer);
    if (innerItems.length > 0) {
      if (!eap.Inner || innerItems.indexOf(eap.Inner) < 0)
        this.set('configProperties_.WiFi.EAP.Inner', innerItems[0]);
    } else {
      delete eap.Inner;
    }
    this.setShowEap_(this.configProperties_.WiFi.EAP);
  },

  /**
   * @param {CrOnc.Type} type The type to compare against.
   * @param {CrOnc.Type} networkType The current network type.
   * @return {boolean} True if the network type matches 'type'.
   * @private
   */
  isType_: function(type, networkType) {
    return type == networkType;
  },

  /**
   * @return {boolean}
   * @private
   */
  saveIsEnabled_: function() {
    return this.propertiesReceived_ && !this.propertiesSent_;
  },

  /**
   * @return {boolean}
   * @private
   */
  connectIsEnabled_: function() {
    if (this.propertiesSent_)
      return false;
    if (this.configProperties_.Type == CrOnc.Type.WI_FI) {
      if (!this.get('WiFi.SSID', this.configProperties_))
        return false;
      if (this.configRequiresPassphrase_()) {
        var passphrase = this.get('WiFi.Passphrase', this.configProperties_);
        if (!passphrase || passphrase.length < this.MIN_PASSPHRASE_LENGTH)
          return false;
      }
    }
    // TODO(stevenjb): Check certificates.
    return true;
  },

  /**
   * @return {boolean}
   * @private
   */
  shareIsEnabled_: function() {
    if (this.networkProperties_.Source == CrOnc.Source.DEVICE ||
        this.networkProperties_.Source == CrOnc.Source.DEVICE_POLICY) {
      return false;
    }
    // TODO(stevenjb): Check login state.

    if (this.configProperties_.Type == CrOnc.Type.WI_FI) {
      var security = this.get('WiFi.Security', this.configProperties_);
      if (!security || security == CrOnc.Security.NONE) {
        return false;
      } else if (security == CrOnc.Security.WPA_EAP) {
        var outer = this.get('WiFi.EAP.Outer', this.configProperties_);
        if (outer == CrOnc.EAPType.EAP_TLS)
          return false;
      }
      // TODO(stevenjb): Check certificates.
    }
    return true;
  },

  /** @private */
  onSaveTap_: function() {
    assert(this.guid_);
    if (this.propertiesSent_)
      return;
    this.propertiesSent_ = true;
    var propertiesToSet = Object.assign({}, this.configProperties_);
    propertiesToSet.GUID = this.guid_;
    this.networkingPrivate.setProperties(
        this.guid_, propertiesToSet, this.setPropertiesCallback_.bind(this));
  },

  /** @private */
  setPropertiesCallback_: function() {
    var error = chrome.runtime.lastError && chrome.runtime.lastError.message;
    if (error) {
      console.error(
          'Error setting network properties: ' + this.guid_ + ': ' + error);
    }
    this.close_();
  },

  /** @private */
  onConnectTap_: function() {
    assert(!this.guid_);
    if (this.propertiesSent_)
      return;
    this.propertiesSent_ = true;
    // Create the configuration, then connect to it in the callback.
    this.networkingPrivate.createNetwork(
        this.shareNetwork_, this.configProperties_,
        this.createNetworkCallback_.bind(this));
  },

  /**
   * @param {string} guid
   * @private
   */
  createNetworkCallback_: function(guid) {
    var error = chrome.runtime.lastError && chrome.runtime.lastError.message;
    if (error) {
      // TODO(stevenjb): Display error message.
      console.error(
          'Error creating network type: ' + this.networkProperties_.Type +
          ': ' + error);
      return;
    }
    this.networkProperties_.GUID = guid;
    this.fire('network-connect', {networkProperties: this.networkProperties_});
    this.close_();
  },

  /** @private */
  onCancelTap_: function() {
    this.close_();
  },

  /**
   * @return boolean
   * @private
   */
  configRequiresPassphrase_: function() {
    if (this.configProperties_.Type != CrOnc.Type.WI_FI)
      return false;
    var security = this.get('WiFi.Security', this.configProperties_);
    return security == CrOnc.Security.WEP_PSK ||
        security == CrOnc.Security.WPA_PSK;
  },

  /**
   * Sets the EAP properties for |eap|, which may be WiFi.EAP, Ethernet.EAP etc.
   * @private
   */
  setShowEap_: function(eap) {
    var outer = eap.Outer;
    this.showEap_ = {
      Inner: outer == CrOnc.EAPType.PEAP || outer == CrOnc.EAPType.EAP_TTLS,
      ServerCA: outer != CrOnc.EAPType.LEAP,
      SubjectMatch: outer == CrOnc.EAPType.EAP_TLS,
      UserCert: outer == CrOnc.EAPType.EAP_TLS,
      Password: outer != CrOnc.EAPType.EAP_TLS,
      AnonymousIdentity:
          outer == CrOnc.EAPType.PEAP || outer == CrOnc.EAPType.EAP_TTLS,
    };
  },

  /**
   * @param {string} outer
   * @return {!Array<string>}
   * @private
   */
  getEapInnerItems_: function(outer) {
    if (outer == CrOnc.EAPType.PEAP)
      return this.eapInnerItemsPeap_;
    if (outer == CrOnc.EAPType.EAP_TTLS)
      return this.eapInnerItemsTtls_;
    return [];
  },
});
