// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing network proxy
 * values.
 */
Polymer({
  is: 'network-proxy',

  behaviors: [
    CrPolicyNetworkBehavior,
    I18nBehavior,
    PrefsBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * The network properties dictionary containing the proxy properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /** Whether or not the proxy values can be edited. */
    editable: {
      type: Boolean,
      value: false,
    },

    /**
     * UI visible / edited proxy configuration.
     * @private {!CrOnc.ProxySettings}
     */
    proxy_: {
      type: Object,
      value: function() {
        return this.createDefaultProxySettings_();
      },
    },

    /**
     * The Web Proxy Auto Discovery URL extracted from networkProperties.
     * @private
     */
    WPAD_: {
      type: String,
      value: '',
    },

    /**
     * Whether or not to use the same manual proxy for all protocols.
     * @private
     */
    useSameProxy_: {
      type: Boolean,
      value: false,
      observer: 'useSameProxyChanged_',
    },

    /**
     * Reflects prefs.settings.use_shared_proxies for data binding.
     * @private
     */
    useSharedProxies_: Boolean,

    /**
     * Array of proxy configuration types.
     * @private {!Array<string>}
     * @const
     */
    proxyTypes_: {
      type: Array,
      value: [
        CrOnc.ProxySettingsType.DIRECT,
        CrOnc.ProxySettingsType.PAC,
        CrOnc.ProxySettingsType.WPAD,
        CrOnc.ProxySettingsType.MANUAL,
      ],
      readOnly: true
    },

    /**
     * Object providing proxy type values for data binding.
     * @private {!Object}
     * @const
     */
    ProxySettingsType_: {
      type: Object,
      value: {
        DIRECT: CrOnc.ProxySettingsType.DIRECT,
        PAC: CrOnc.ProxySettingsType.PAC,
        MANUAL: CrOnc.ProxySettingsType.MANUAL,
        WPAD: CrOnc.ProxySettingsType.WPAD,
      },
      readOnly: true,
    },
  },

  observers: [
    'useSharedProxiesChanged_(prefs.settings.use_shared_proxies.value)',
  ],

  /**
   * Saved Manual properties so that switching to another type does not loose
   * any set properties while the UI is open.
   * @private {!CrOnc.ManualProxySettings|undefined}
   */
  savedManual_: undefined,

  /**
   * Saved ExcludeDomains properties so that switching to a non-Manual type does
   * not loose any set exclusions while the UI is open.
   * @private {!Array<string>|undefined}
   */
  savedExcludeDomains_: undefined,

  /**
   * Set to true while modifying proxy values so that an update does not
   * override the edited values.
   * @private {boolean}
   */
  proxyModified_: false,

  /** @protected settings.RouteObserverBehavior */
  currentRouteChanged: function(newRoute) {
    this.proxyModified_ = false;
    this.proxy_ = this.createDefaultProxySettings_();
    if (newRoute == settings.Route.NETWORK_DETAIL)
      this.updateProxy_();
  },

  /** @private */
  networkPropertiesChanged_: function() {
    if (this.proxyModified_)
      return;  // Ignore update.
    this.updateProxy_();
  },

  /** @private */
  updateProxy_: function() {
    if (!this.networkProperties)
      return;

    /** @type {!CrOnc.ProxySettings} */
    var proxy = this.createDefaultProxySettings_();

    // For shared networks with unmanaged proxy settings, ignore any saved
    // proxy settings (use the default values).
    if (this.isShared_()) {
      var property = this.getProxySettingsTypeProperty_();
      if (!this.isControlled(property) && !this.useSharedProxies_) {
        this.setProxyAsync_(proxy);
        return;  // Proxy settings will be ignored.
      }
    }

    /** @type {!chrome.networkingPrivate.ManagedProxySettings|undefined} */
    var proxySettings = this.networkProperties.ProxySettings;
    if (proxySettings) {
      proxy.Type = /** @type {!CrOnc.ProxySettingsType} */ (
          CrOnc.getActiveValue(proxySettings.Type));
      if (proxySettings.Manual) {
        proxy.Manual.HTTPProxy = /** @type {!CrOnc.ProxyLocation|undefined} */ (
                                     CrOnc.getSimpleActiveProperties(
                                         proxySettings.Manual.HTTPProxy)) ||
            {Host: '', Port: 80};
        proxy.Manual.SecureHTTPProxy =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(
                    proxySettings.Manual.SecureHTTPProxy)) ||
            proxy.Manual.HTTPProxy;
        proxy.Manual.FTPProxy =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(
                    proxySettings.Manual.FTPProxy)) ||
            proxy.Manual.HTTPProxy;
        proxy.Manual.SOCKS =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(proxySettings.Manual.SOCKS)) ||
            proxy.Manual.HTTPProxy;
        var jsonHttp = proxy.Manual.HTTPProxy;
        this.useSameProxy_ =
            CrOnc.proxyMatches(jsonHttp, proxy.Manual.SecureHTTPProxy) &&
            CrOnc.proxyMatches(jsonHttp, proxy.Manual.FTPProxy) &&
            CrOnc.proxyMatches(jsonHttp, proxy.Manual.SOCKS);
      }
      if (proxySettings.ExcludeDomains) {
        proxy.ExcludeDomains = /** @type {!Array<string>|undefined} */ (
            CrOnc.getActiveValue(proxySettings.ExcludeDomains));
      }
      proxy.PAC = /** @type {string|undefined} */ (
          CrOnc.getActiveValue(proxySettings.PAC));
    }
    // Use saved ExcludeDomanains and Manual if not defined.
    proxy.ExcludeDomains = proxy.ExcludeDomains || this.savedExcludeDomains_;
    proxy.Manual = proxy.Manual || this.savedManual_;

    // Set the Web Proxy Auto Discovery URL.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    this.WPAD_ = (ipv4 && ipv4.WebProxyAutoDiscoveryUrl) || '';

    this.setProxyAsync_(proxy);
  },

  /**
   * @param {!CrOnc.ProxySettings} proxy
   * @private
   */
  setProxyAsync_: function(proxy) {
    // Set this.proxy_ after dom-repeat has been stamped.
    this.async(function() {
      this.proxy_ = proxy;
      this.proxyModified_ = false;
    }.bind(this));
  },

  /** @private */
  useSameProxyChanged_: function() {
    this.proxyModified_ = true;
  },

  /** @private */
  useSharedProxiesChanged_: function() {
    var pref = this.getPref('settings.use_shared_proxies');
    this.useSharedProxies_ = !!pref && !!pref.value;
    this.updateProxy_();
  },

  /**
   * @return {CrOnc.ProxySettings} An empty/default proxy settings object.
   * @private
   */
  createDefaultProxySettings_: function() {
    return {
      Type: CrOnc.ProxySettingsType.DIRECT,
      ExcludeDomains: [],
      Manual: {
        HTTPProxy: {Host: '', Port: 80},
        SecureHTTPProxy: {Host: '', Port: 80},
        FTPProxy: {Host: '', Port: 80},
        SOCKS: {Host: '', Port: 1080}
      },
      PAC: ''
    };
  },

  /**
   * Called when the proxy changes in the UI.
   * @private
   */
  sendProxyChange_: function() {
    if (this.proxy_.Type == CrOnc.ProxySettingsType.MANUAL) {
      var proxy =
          /** @type {!CrOnc.ProxySettings} */ (Object.assign({}, this.proxy_));
      var manual = proxy.Manual;
      var defaultProxy = manual.HTTPProxy;
      if (!defaultProxy || !defaultProxy.Host)
        return;
      if (this.useSameProxy_ || !this.get('SecureHTTPProxy.Host', manual)) {
        proxy.Manual.SecureHTTPProxy = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      }
      if (this.useSameProxy_ || !this.get('FTPProxy.Host', manual)) {
        proxy.Manual.FTPProxy = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      }
      if (this.useSameProxy_ || !this.get('SOCKS.Host', manual)) {
        proxy.Manual.SOCKS = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      }
      this.savedManual_ = Object.assign({}, proxy.Manual);
      this.savedExcludeDomains_ = proxy.ExcludeDomains;
      this.proxy_ = proxy;
    } else if (this.proxy_.Type == CrOnc.ProxySettingsType.PAC) {
      if (!this.proxy_.PAC)
        return;
    }
    this.fire('proxy-change', {field: 'ProxySettings', value: this.proxy_});
    this.proxyModified_ = false;
  },

  /**
   * Event triggered when the selected proxy type changes.
   * @param {!Event} event
   * @private
   */
  onTypeChange_: function(event) {
    var target = /** @type {!HTMLSelectElement} */ (event.target);
    var type = /** @type {chrome.networkingPrivate.ProxySettingsType} */ (
        target.value);
    this.set('proxy_.Type', type);
    if (type == CrOnc.ProxySettingsType.MANUAL)
      this.proxyModified_ = true;
    else
      this.sendProxyChange_();
  },

  /** @private */
  onPACChange_: function() {
    this.sendProxyChange_();
  },

  /** @private */
  onProxyInputChange_: function() {
    this.proxyModified_ = true;
  },

  /**
   * Event triggered when a proxy exclusion is added.
   * @param {Event} event The add proxy exclusion event.
   * @private
   */
  onAddProxyExclusionTap_: function(event) {
    var value = this.$.proxyExclusion.value;
    if (!value)
      return;
    this.push('proxy_.ExcludeDomains', value);
    // Clear input.
    this.$.proxyExclusion.value = '';
    this.proxyModified_ = true;
  },

  /**
   * Event triggered when the proxy exclusion list changes.
   * @param {Event} event The remove proxy exclusions change event.
   * @private
   */
  onProxyExclusionsChange_: function(event) {
    this.proxyModified_ = true;
  },

  /** @private */
  onSaveProxyTap_: function() {
    this.sendProxyChange_();
  },

  /**
   * @param {string} proxyType The proxy type.
   * @return {string} The description for |proxyType|.
   * @private
   */
  getProxyTypeDesc_: function(proxyType) {
    if (proxyType == CrOnc.ProxySettingsType.MANUAL)
      return this.i18n('networkProxyTypeManual');
    if (proxyType == CrOnc.ProxySettingsType.PAC)
      return this.i18n('networkProxyTypePac');
    if (proxyType == CrOnc.ProxySettingsType.WPAD)
      return this.i18n('networkProxyTypeWpad');
    return this.i18n('networkProxyTypeDirect');
  },

  /**
   * @return {!CrOnc.ManagedProperty|undefined}
   * @private
   */
  getProxySettingsTypeProperty_: function() {
    return /** @type {!CrOnc.ManagedProperty|undefined} */ (
        this.get('ProxySettings.Type', this.networkProperties));
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNetworkPolicyIndicator_: function() {
    var property = this.getProxySettingsTypeProperty_();
    return !!property && !this.isExtensionControlled(property) &&
        this.isNetworkPolicyEnforced(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowExtensionIndicator_: function() {
    var property = this.getProxySettingsTypeProperty_();
    return !!property && this.isExtensionControlled(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty} property
   * @return {boolean}
   * @private
   */
  shouldShowAllowShared_: function(property) {
    return !this.isControlled(property) && this.isShared_();
  },

  /**
   * @param {string} propertyName
   * @return {boolean} Whether the named property setting is editable.
   * @private
   */
  isEditable_: function(propertyName) {
    if (!this.editable || (this.isShared_() && !this.useSharedProxies_))
      return false;
    if (!this.networkProperties.hasOwnProperty('ProxySettings'))
      return true;  // No proxy settings defined, so not enforced.
    var property = /** @type {!CrOnc.ManagedProperty|undefined} */ (
        this.get('ProxySettings.' + propertyName, this.networkProperties));
    if (!property)
      return true;
    return this.isPropertyEditable_(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} Whether |property| is editable.
   * @private
   */
  isPropertyEditable_: function(property) {
    return !this.isNetworkPolicyEnforced(property) &&
        !this.isExtensionControlled(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  isShared_: function() {
    return this.networkProperties.Source == 'Device' ||
        this.networkProperties.Source == 'DevicePolicy';
  },

  /**
   * @return {boolean}
   * @private
   */
  isSaveManualProxyEnabled_: function() {
    if (!this.proxyModified_)
      return false;
    return !!this.get('HTTPProxy.Host', this.proxy_.Manual);
  },

  /**
   * @param {string} property The property to test
   * @param {string} value The value to test against
   * @return {boolean} True if property == value
   * @private
   */
  matches_: function(property, value) {
    return property == value;
  },

  /**
   * Handles the change event for the shared proxy checkbox. Shows a
   * confirmation dialog.
   * @param {Event} event
   * @private
   */
  onAllowSharedProxiesChange_: function(event) {
    this.$.confirmAllowSharedDialog.showModal();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Confirm' button.
   * @private
   */
  onAllowSharedDialogConfirm_: function() {
    /** @type {!SettingsCheckboxElement} */ (this.$.allowShared)
        .sendPrefChange();
    this.$.confirmAllowSharedDialog.close();
  },

  /**
   * Handles the shared proxy confirmation dialog 'Cancel' button or a cancel
   * event.
   * @private
   */
  onAllowSharedDialogCancel_: function() {
    /** @type {!SettingsCheckboxElement} */ (this.$.allowShared)
        .resetToPrefValue();
    this.$.confirmAllowSharedDialog.close();
  },
});
