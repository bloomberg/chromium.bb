// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-kerberos-accounts' is the settings subpage containing controls to
 * list, add and delete Kerberos Accounts.
 */

Polymer({
  is: 'settings-kerberos-accounts',

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @private {!Array<!settings.KerberosAccount>}
     */
    accounts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The targeted account for menu operations.
     * @private {?settings.KerberosAccount}
     */
    actionMenuAccount_: Object,
  },

  /** @private {?settings.KerberosAccountsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'kerberos-accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready: function() {
    this.browserProxy_ =
        settings.KerberosAccountsBrowserProxyImpl.getInstance();
    this.refreshAccounts_();
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!Event} event
   * @private
   */
  addAccount_: function(event) {
    this.browserProxy_.addAccount();
  },

  /**
   * @param {!CustomEvent<!{model: !{item: !settings.Account}}>} event
   * @private
   */
  onReauthenticationClick_: function(event) {
    this.browserProxy_.reauthenticateAccount(event.model.item.principalName);
  },

  /**
   * @private
   */
  refreshAccounts_: function() {
    this.browserProxy_.getAccounts().then(accounts => {
      this.accounts_ = accounts;
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: !settings.KerberosAccount}, target: !Element}}
   *      event
   * @private
   */
  onAccountActionsMenuButtonClick_: function(event) {
    this.actionMenuAccount_ = event.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('cr-action-menu'))
        .showAt(event.target);
  },

  /**
   * Closes action menu and resets action menu model.
   * @private
   */
  closeActionMenu_: function() {
    this.$$('cr-action-menu').close();
    this.actionMenuAccount_ = null;
  },

  /**
   * Removes the account being pointed to by |this.actionMenuAccount_|.
   * @private
   */
  onRemoveAccountClick_: function() {
    this.browserProxy_.removeAccount(
        /** @type {!settings.KerberosAccount} */ (this.actionMenuAccount_));
    this.closeActionMenu_();
  }
});
