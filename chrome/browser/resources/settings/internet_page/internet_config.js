// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-config-dialog' is a Settings dialog wrapper for network-config.
 */
Polymer({
  is: 'internet-config-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /** @private */
    shareAllowEnable_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('shareNetworkAllowEnable');
      }
    },

    /** @private */
    shareDefault_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('shareNetworkDefault');
      }
    },

    /**
     * The GUID when an existing network is being configured. This will be
     * empty when configuring a new network.
     * @private
     */
    guid: String,

    /**
     * The type of network to be configured.
     * @private {!chrome.networkingPrivate.NetworkType}
     */
    type: String,

    /**
     * The name of network (for display while the network details are fetched).
     * @private
     */
    name: String,

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

  open: function() {
    var dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open)
      dialog.showModal();

    // Set networkProperties for new configurations and for existing
    // configurations until the current properties are loaded.
    assert(this.type && this.type != CrOnc.Type.ALL);
    this.networkProperties_ = {
      GUID: this.guid,
      Name: this.name,
      Type: this.type,
    };
    this.$.networkConfig.init();
  },

  close: function() {
    var dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open)
      dialog.close();
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
    return !!this.guid && !!source && source != CrOnc.Source.NONE;
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
    this.close();
  },

  /** @private */
  onSaveOrConnectTap_: function() {
    this.$.networkConfig.saveOrConnect();
  },
});
