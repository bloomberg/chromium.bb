// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-internet-config' is a Settings wrapper for network-config.
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
     * The type of network is being configured.
     * @private {!chrome.networkingPrivate.NetworkType}
     */
    type_: String,

    /**
     * The network of the network being configured. This is 2-way data bound to
     * |name| in the network-config element which will set the property.
     * @private
     */
    name_: String,

    /** @private */
    enableConnect_: String,

    /** @private */
    enableSave_: String,

    /**
     * The current properties if an existing network being configured, or
     * a minimal subset for a new network.
     * @private {!chrome.networkingPrivate.NetworkProperties|undefined}
     */
    networkProperties_: Object,
  },

  /**
   * settings.RouteObserverBehavior
   * @param {!settings.Route} route
   * @protected
   */
  currentRouteChanged: function(route) {
    if (route != settings.routes.NETWORK_CONFIG)
      return;

    var queryParams = settings.getQueryParameters();
    this.guid_ = queryParams.get('guid') || '';

    // Set networkProperties for new configurations and for existing
    // configurations until the current properties are loaded.
    this.name_ = queryParams.get('name') || '';
    var typeParam = queryParams.get('type');
    this.type_ =
        (typeParam && CrOnc.getValidType(typeParam)) || CrOnc.Type.WI_FI;
    assert(this.type_ && this.type_ != CrOnc.Type.ALL);
    this.networkProperties_ = {
      GUID: this.guid_,
      Name: this.name_,
      Type: this.type_,
    };

    // First focus this page (which will focus a button), then init the config
    // element which will focus an enabled element if any.
    this.focus();
    this.$.networkConfig.init();
  },

  focus() {
    var e = this.$$('paper-button:not([disabled])');
    assert(e);  // The 'cancel' button should never be disabled.
    e.focus();
  },

  /** @private */
  close_: function() {
    if (settings.getCurrentRoute() == settings.routes.NETWORK_CONFIG)
      settings.navigateToPreviousRoute();
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_: function() {
    return this.name_ || this.i18n('OncType' + this.type_);
  },

  /** @private */
  onCancelTap_: function() {
    this.close_();
  },

  /** @private */
  onSaveTap_: function() {
    this.$.networkConfig.saveOrConnect();
  },

  /** @private */
  onConnectTap_: function() {
    this.$.networkConfig.saveOrConnect();
  },
});
