// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing network proxy
 * values.
 */
Polymer({
  is: 'network-proxy',

  behaviors: [CrPolicyNetworkBehavior, I18nBehavior, PrefsBehavior],

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
     * @type {!CrOnc.ProxySettings}
     */
    proxy: {
      type: Object,
      value: function() {
        return this.createDefaultProxySettings_();
      },
    },

    /** The Web Proxy Auto Discovery URL extracted from networkProperties. */
    WPAD: {
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
   * Set to true the first time we receive a manual proxy. Used to set the
   * initial |useSameProxy_| value.
   * @private {boolean}
   */
  receivedManualProxy_: false,

  /** @private */
  networkPropertiesChanged_: function() {
    if (!this.networkProperties)
      return;

    /** @type {!CrOnc.ProxySettings} */
    var proxy = this.createDefaultProxySettings_();
    /** @type {!chrome.networkingPrivate.ManagedProxySettings|undefined} */
    var proxySettings = this.networkProperties.ProxySettings;
    if (proxySettings) {
      proxy.Type = /** @type {!CrOnc.ProxySettingsType} */ (
          CrOnc.getActiveValue(proxySettings.Type));
      if (proxySettings.Manual) {
        proxy.Manual.HTTPProxy = /** @type {!CrOnc.ProxyLocation|undefined} */ (
            CrOnc.getSimpleActiveProperties(proxySettings.Manual.HTTPProxy));
        proxy.Manual.SecureHTTPProxy =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(
                    proxySettings.Manual.SecureHTTPProxy));
        proxy.Manual.FTPProxy = /** @type {!CrOnc.ProxyLocation|undefined} */ (
            CrOnc.getSimpleActiveProperties(proxySettings.Manual.FTPProxy));
        proxy.Manual.SOCKS = /** @type {!CrOnc.ProxyLocation|undefined} */ (
            CrOnc.getSimpleActiveProperties(proxySettings.Manual.SOCKS));
        if (!this.receivedManualProxy_) {
          let json_http = JSON.stringify(proxy.Manual.HTTPProxy);
          this.useSameProxy_ =
              json_http == JSON.stringify(proxy.Manual.SecureHTTPProxy) &&
              json_http == JSON.stringify(proxy.Manual.FTPProxy) &&
              json_http == JSON.stringify(proxy.Manual.SOCKS);
          this.receivedManualProxy_ = true;
        }
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

    // Set this.proxy after dom-repeat has been stamped.
    this.async(function() {
      this.proxy = proxy;
    }.bind(this));

    // Set the Web Proxy Auto Discovery URL.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    this.WPAD = (ipv4 && ipv4.WebProxyAutoDiscoveryUrl) || '';
  },

  /** @private */
  useSameProxyChanged_: function() {
    if (!this.receivedManualProxy_)
      return;
    this.sendProxyChange_();
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
    if (this.proxy.Type == CrOnc.ProxySettingsType.MANUAL) {
      var proxy =
          /** @type {!CrOnc.ProxySettings} */ (Object.assign({}, this.proxy));
      var defaultProxy = proxy.Manual.HTTPProxy;
      if (!defaultProxy || !defaultProxy.Host)
        return;
      if (this.useSameProxy_ || !proxy.Manual.SecureHTTPProxy) {
        proxy.Manual.SecureHTTPProxy = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      }
      if (this.useSameProxy_ || !proxy.Manual.FTPProxy) {
        proxy.Manual.FTPProxy = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      }
      if (this.useSameProxy_ || !proxy.Manual.SOCKS) {
        proxy.Manual.SOCKS = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      }
      this.savedManual_ = Object.assign({}, proxy.Manual);
      this.savedExcludeDomains_ = proxy.ExcludeDomains;
      this.proxy = proxy;
    } else if (this.proxy.Type == CrOnc.ProxySettingsType.PAC) {
      if (!this.proxy.PAC)
        return;
    }
    this.fire('proxy-change', {field: 'ProxySettings', value: this.proxy});
  },

  /**
   * Event triggered when the selected proxy type changes.
   * @param {!Event} event
   * @private
   */
  onTypeChange_: function(event) {
    let target = /** @type {!HTMLSelectElement} */ (event.target);
    var type = /** @type {chrome.networkingPrivate.ProxySettingsType} */ (
        target.value);
    this.set('proxy.Type', type);
    this.sendProxyChange_();
  },

  /**
   * Event triggered when a proxy value changes.
   * @param {Event} event The proxy value change event.
   * @private
   */
  onProxyInputChange_: function(event) {
    this.sendProxyChange_();
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
    this.push('proxy.ExcludeDomains', value);
    // Clear input.
    this.$.proxyExclusion.value = '';
    this.sendProxyChange_();
  },

  /**
   * Event triggered when the proxy exclusion list changes.
   * @param {Event} event The remove proxy exclusions change event.
   * @private
   */
  onProxyExclusionsChange_: function(event) {
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
   * @return {boolean}
   * @private
   */
  getShowNetworkPolicyIndicator_: function() {
    let property = /** @type {!CrOnc.ManagedProperty|undefined}*/ (
        this.get('ProxySettings.Type', this.networkProperties));
    return !!property && !this.isExtensionControlled(property) &&
        this.isNetworkPolicyEnforced(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  getShowPrefPolicyIndicator_: function() {
    let property = /** @type {!CrOnc.ManagedProperty|undefined}*/ (
        this.get('ProxySettings.Type', this.networkProperties));
    return !!property && this.isExtensionControlled(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} Whether the property setting is enforced.
   * @private
   */
  isEditable_: function(property) {
    return this.editable && !this.isNetworkPolicyEnforced(property) &&
        !this.isExtensionControlled(property);
  },

  /**
   * Used to check the editable state for proxy related UI that may or may
   * not be directly controlled by a policy. We use the enforced state of the
   * 'ProxySettings.Type' property for these controls.
   * @return {boolean} Whether the proxy control is editable.
   * @private
   */
  isProxyEditable_: function() {
    let property = /** @type {!CrOnc.ManagedProperty|undefined}*/ (
        this.get('ProxySettings.Type', this.networkProperties));
    return !!property && this.isEditable_(property);
  },

  /**
   * @param {string} property The property to test
   * @param {string} value The value to test against
   * @return {boolean} True if property == value
   * @private
   */
  matches_: function(property, value) {
    return property == value;
  }
});
