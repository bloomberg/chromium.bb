// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-config' is a Settings dialog wrapper for network-config.
 */
Polymer({
  is: 'internet-config',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls, passed from internet_page.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: Object,

    /** @type {!chrome.networkingPrivate.GlobalPolicy|undefined} */
    globalPolicy: Object,

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

    /**
     * Set by network-config when a configuration error occurs.
     * @private
     */
    error_: {
      type: String,
      value: '',
    },
  },

  open: function() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
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
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open)
      dialog.close();
  },

  /**
   * @return {string}
   * @private
   */
  getDialogTitle_: function() {
    const name = this.networkProperties_.Name;
    if (name)
      return this.i18n('internetConfigName', HTMLEscape(name));
    const type = this.i18n('OncType' + this.networkProperties_.Type);
    return this.i18n('internetJoinType', type);
  },

  /**
   * @return {string}
   * @private
   */
  getError_: function() {
    if (this.i18nExists(this.error_))
      return this.i18n(this.error_);
    return this.i18n('networkErrorUnknown');
  },

  /**
   * @return {boolean}
   * @private
   */
  isConfigured_: function() {
    const source = this.networkProperties_.Source;
    return !!this.guid && !!source && source != CrOnc.Source.NONE;
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
