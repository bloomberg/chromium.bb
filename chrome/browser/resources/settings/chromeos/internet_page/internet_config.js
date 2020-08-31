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
    /** @private */
    shareAllowEnable_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shareNetworkAllowEnable');
      }
    },

    /** @private */
    shareDefault_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('shareNetworkDefault');
      }
    },

    /**
     * The GUID when an existing network is being configured. This will be
     * empty when configuring a new network.
     */
    guid: String,

    /**
     * The type of network to be configured as a string. May be set initially or
     * updated by network-config.
     */
    type: String,

    /**
     * The name of the network. May be set initially or updated by
     * network-config.
     */
    name: String,

    /**
     * Set to true to show the 'connect' button instead of 'save'.
     */
    showConnect: Boolean,

    /** @private */
    enableConnect_: Boolean,

    /** @private */
    enableSave_: Boolean,

    /**
     * Set by network-config when a configuration error occurs.
     * @private
     */
    error_: {
      type: String,
      value: '',
    },
  },

  open() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (!dialog.open) {
      dialog.showModal();
    }

    this.$.networkConfig.init();
  },

  close() {
    const dialog = /** @type {!CrDialogElement} */ (this.$.dialog);
    if (dialog.open) {
      dialog.close();
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onClose_(event) {
    this.close();
  },

  /**
   * @return {string}
   * @private
   */
  getDialogTitle_() {
    if (this.name && !this.showConnect) {
      return this.i18n('internetConfigName', HTMLEscape(this.name));
    }
    const type = this.i18n('OncType' + this.type);
    return this.i18n('internetJoinType', type);
  },

  /**
   * @return {string}
   * @private
   */
  getError_() {
    if (this.i18nExists(this.error_)) {
      return this.i18n(this.error_);
    }
    return this.i18n('networkErrorUnknown');
  },

  /** @private */
  onCancelTap_() {
    this.close();
  },

  /** @private */
  onSaveTap_() {
    this.$.networkConfig.save();
    settings.recordSettingChange();
  },

  /** @private */
  onConnectTap_() {
    this.$.networkConfig.connect();
    settings.recordSettingChange();
  },
});
