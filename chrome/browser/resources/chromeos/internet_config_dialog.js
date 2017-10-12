// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'internet-config-dialog' is used to configure a new or existing network
 * outside of settings (e.g. from the login screen or when configuring a
 * new network from the system tray).
 */
Polymer({
  is: 'internet-config-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * Interface for networkingPrivate calls.
     * @type {NetworkingPrivate}
     */
    networkingPrivate: {
      type: Object,
      value: chrome.networkingPrivate,
    },

    /**
     * The network GUID to configure, or empty when configuring a new network.
     * @private
     */
    guid_: String,

    /** @private */
    enableConnect_: String,

    /** @private */
    enableSave_: String,

    /**
     * The current properties if an existing network is being configured, or
     * a minimal subset for a new network. Note: network-config may modify
     * this (specifically .name).
     * @type {!chrome.networkingPrivate.NetworkProperties}
     */
    networkProperties_: Object,
  },

  /** @override */
  attached: function() {
    var dialogArgs = chrome.getVariableValue('dialogArguments');
    assert(dialogArgs);
    var args = JSON.parse(dialogArgs);
    var type = /** @type {chrome.networkingPrivate.NetworkType} */ (args.type);
    assert(type);
    this.guid_ = args.guid || '';

    this.networkProperties_ = {
      GUID: this.guid_,
      Name: '',
      Type: type,
    };

    this.$.networkConfig.init();
  },

  /** @private */
  close_: function() {
    chrome.send('dialogClose');
  },

  /**
   * @return {string}
   * @private
   */
  getTitle_: function() {
    return this.networkProperties_.Name ||
        this.i18n('OncType' + this.networkProperties_.Type);
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
