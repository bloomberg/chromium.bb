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

    /** @private */
    enableConnect_: Boolean,

    /** @private */
    enableSave_: Boolean,

    /**
     * The current properties if an existing network is being configured, or
     * a minimal subset for a new network. Note: network-config may modify
     * this (specifically .name).
     * @private {!chrome.networkingPrivate.NetworkProperties}
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
    var name = queryParams.get('name') || '';
    var typeParam = queryParams.get('type');
    var type = (typeParam && CrOnc.getValidType(typeParam)) || CrOnc.Type.WI_FI;
    assert(type && type != CrOnc.Type.ALL);
    this.networkProperties_ = {
      GUID: this.guid_,
      Name: name,
      Type: type,
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
    return this.networkProperties_.Name ||
        this.i18n('OncType' + this.networkProperties_.Type);
  },

  /**
   * @return {boolean}
   * @private
   */
  isConfigured_: function() {
    var source = this.networkProperties_.Source;
    return !!this.guid_ && !!source && source != CrOnc.Source.NONE;
  },

  /**
   * @return {string}
   * @private
   */
  getSaveOrConnectLabel_: function() {
    return this.i18n(this.isConfigured_() ? 'save' : 'networkButtonConnect');
  },

  /**
   * @return {boolean}
   * @private
   */
  getSaveOrConnectEnabled_: function() {
    return this.isConfigured_() ? this.enableSave_ : this.enableConnect_;
  },

  /** @private */
  onCancelTap_: function() {
    this.close_();
  },

  /** @private */
  onSaveOrConnectTap_: function() {
    this.$.networkConfig.saveOrConnect();
  },
});
