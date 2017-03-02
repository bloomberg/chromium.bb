// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-page' is the settings page containing internet
 * settings.
 */
Polymer({
  is: 'settings-internet-page',

  behaviors: [I18nBehavior, settings.RouteObserverBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls. May be overriden by tests.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The device state for each network device type. Set by network-summary.
     * @type {!Object<chrome.networkingPrivate.DeviceStateProperties>|undefined}
     * @private
     */
    deviceStates: {
      type: Object,
      notify: true,
    },

    /**
     * Highest priority connected network or null. Set by network-summary.
     * @type {?CrOnc.NetworkStateProperties|undefined}
     */
    defaultNetwork: {
      type: Object,
      notify: true,
    },

    /**
     * Set by internet-subpage. Controls spinner visibility in subpage header.
     * @private
     */
    showSpinner_: Boolean,

    /**
     * The network type for the networks subpage. Used in the subpage header.
     * @private
     */
    subpageType_: String,

    /**
     * The network type for the known networks subpage.
     * @private
     */
    knownNetworksType_: String,

    /**
     * Whether the 'Add connection' section is expanded.
     * @private
     */
    addConnectionExpanded_: {
      type: Boolean,
      value: false,
    },

    /** @private {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy_: Object,

    /**
     * List of third party VPN providers.
     * @type {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}
     * @private
     */
    thirdPartyVpnProviders_: {
      type: Array,
      value: function() {
        return [];
      }
    },
  },

  // Element event listeners
  listeners: {
    'device-enabled-toggled': 'onDeviceEnabledToggled_',
    'show-detail': 'onShowDetail_',
    'show-known-networks': 'onShowKnownNetworks_',
    'show-networks': 'onShowNetworks_',
  },

  // chrome.networkingPrivate listeners
  /** @private {Function} */
  onExtensionAddedListener_: null,

  /** @private {Function} */
  onExtensionRemovedListener_: null,

  /** @private {Function} */
  onExtensionDisabledListener_: null,

  /** @override */
  attached: function() {
    this.onExtensionAddedListener_ = this.onExtensionAddedListener_ ||
        this.onExtensionAdded_.bind(this);
    chrome.management.onInstalled.addListener(this.onExtensionAddedListener_);
    chrome.management.onEnabled.addListener(this.onExtensionAddedListener_);

    this.onExtensionRemovedListener_ = this.onExtensionRemovedListener_ ||
        this.onExtensionRemoved_.bind(this);
    chrome.management.onUninstalled.addListener(
        this.onExtensionRemovedListener_);

    this.onExtensionDisabledListener_ = this.onExtensionDisabledListener_ ||
        this.onExtensionDisabled_.bind(this);
    chrome.management.onDisabled.addListener(this.onExtensionDisabledListener_);

    chrome.management.getAll(this.onGetAllExtensions_.bind(this));

    this.networkingPrivate.getGlobalPolicy(function(policy) {
      this.globalPolicy_ = policy;
    }.bind(this));
  },

  /** @override */
  detached: function() {
    chrome.management.onInstalled.removeListener(
        assert(this.onExtensionAddedListener_));
    chrome.management.onEnabled.removeListener(
        assert(this.onExtensionAddedListener_));
    chrome.management.onUninstalled.removeListener(
        assert(this.onExtensionRemovedListener_));
    chrome.management.onDisabled.removeListener(
        assert(this.onExtensionDisabledListener_));
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route == settings.Route.INTERNET_NETWORKS) {
      // Handle direct navigation to the networks page,
      // e.g. chrome://settings/internet/networks?type=WiFi
      var queryParams = settings.getQueryParameters();
      var type = queryParams.get('type');
      if (type)
        this.subpageType_ = type;
    }
  },

  /**
   * Event triggered by a device state enabled toggle.
   * @param {!{detail: {enabled: boolean,
   *                    type: chrome.networkingPrivate.NetworkType}}} event
   * @private
   */
  onDeviceEnabledToggled_: function(event) {
    if (event.detail.enabled)
      this.networkingPrivate.enableNetworkType(event.detail.type);
    else
      this.networkingPrivate.disableNetworkType(event.detail.type);
  },

  /**
   * @param {!{detail: !CrOnc.NetworkStateProperties}} event
   * @private
   */
  onShowDetail_: function(event) {
    var params = new URLSearchParams;
    params.append('guid', event.detail.GUID);
    params.append('type', event.detail.Type);
    if (event.detail.Name)
      params.append('name', event.detail.Name);
    settings.navigateTo(settings.Route.NETWORK_DETAIL, params);
  },

  /**
   * @param {!{detail: {type: string}}} event
   * @private
   */
  onShowNetworks_: function(event) {
    var params = new URLSearchParams;
    params.append('type', event.detail.Type);
    this.subpageType_ = event.detail.Type;
    settings.navigateTo(settings.Route.INTERNET_NETWORKS, params);
  },

  /**
   * @return {string}
   * @private
   */
  getNetworksPageTitle_: function() {
    return this.i18n('OncType' + this.subpageType_);
  },

  /**
   * @param {!{detail: {type: string}}} event
   * @private
   */
  onShowKnownNetworks_: function(event) {
    this.knownNetworksType_ = event.detail.type;
    settings.navigateTo(settings.Route.KNOWN_NETWORKS);
  },

  /**
   * Event triggered when the 'Add connections' div is tapped.
   * @param {Event} event
   * @private
   */
  onExpandAddConnectionsTap_: function(event) {
    if (event.target.id == 'expandAddConnections')
      return;
    this.addConnectionExpanded_ = !this.addConnectionExpanded_;
  },

  /** @private */
  onAddWiFiTap_: function() {
    chrome.send('addNetwork', [CrOnc.Type.WI_FI]);
  },

  /** @private */
  onAddVPNTap_: function() {
    chrome.send('addNetwork', [CrOnc.Type.VPN]);
  },

  /**
   * @param {!{model:
   *            !{item: !chrome.networkingPrivate.ThirdPartyVPNProperties},
   *        }} event
   * @private
   */
  onAddThirdPartyVpnTap_: function(event) {
    var provider = event.model.item;
    chrome.send('addNetwork', [CrOnc.Type.VPN, provider.ExtensionID]);
  },

  /**
   * chrome.management.getAll callback.
   * @param {!Array<!chrome.management.ExtensionInfo>} extensions
   * @private
   */
  onGetAllExtensions_: function(extensions) {
    var vpnProviders = [];
    for (var i = 0; i < extensions.length; ++i)
      this.addVpnProvider_(vpnProviders, extensions[i]);
    this.thirdPartyVpnProviders_ = vpnProviders;
  },

  /**
   * If |extension| is a third-party VPN provider, add it to |vpnProviders|.
   * @param {!Array<!chrome.networkingPrivate.ThirdPartyVPNProperties>}
   *     vpnProviders
   * @param {!chrome.management.ExtensionInfo} extension
   * @private
   */
  addVpnProvider_: function(vpnProviders, extension) {
    if (!extension.enabled ||
        extension.permissions.indexOf('vpnProvider') == -1) {
      return;
    }
    if (vpnProviders.find(function(provider) {
          return provider.ExtensionID == extension.id;
        })) {
      return;
    }
    var newProvider = {
      ExtensionID: extension.id,
      ProviderName: extension.name,
    };
    vpnProviders.push(newProvider);
  },

  /**
   * chrome.management.onInstalled or onEnabled event.
   * @param {!chrome.management.ExtensionInfo} extension
   * @private
   */
  onExtensionAdded_: function(extension) {
    this.addVpnProvider_(this.thirdPartyVpnProviders_, extension);
  },

  /**
   * chrome.management.onUninstalled event.
   * @param {string} extensionId
   * @private
   */
  onExtensionRemoved_: function(extensionId) {
    for (var i = 0; i < this.thirdPartyVpnProviders_.length; ++i) {
      var provider = this.thirdPartyVpnProviders_[i];
      if (provider.ExtensionID == extensionId) {
        this.splice('thirdPartyVpnProviders_', i, 1);
        break;
      }
    }
  },

  /**
   * chrome.management.onDisabled event.
   * @param {{id: string}} extension
   * @private
   */
  onExtensionDisabled_: function(extension) {
    this.onExtensionRemoved_(extension.id);
  },

  /**
   * @param {!chrome.networkingPrivate.DeviceStateProperties} deviceState
   * @return {boolean}
   * @private
   */
  deviceIsEnabled_: function(deviceState) {
    return deviceState.State ==
        chrome.networkingPrivate.DeviceStateType.ENABLED;
  },

  /**
   * @param {!chrome.networkingPrivate.GlobalPolicy} globalPolicy
   * @return {boolean}
   */
  allowAddConnection_: function(globalPolicy) {
    return !globalPolicy.AllowOnlyPolicyNetworksToConnect;
  },

  /**
   * @param {!chrome.networkingPrivate.ThirdPartyVPNProperties} provider
   * @return {string}
   */
  getAddThirdParrtyVpnLabel_: function(provider) {
    return this.i18n('internetAddThirdPartyVPN', provider.ProviderName);
  }
});
