// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-account-manager' is the settings subpage containing controls to
 * list, add and delete Secondary Google Accounts.
 */

Polymer({
  is: 'settings-account-manager',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /**
     * List of Accounts.
     * @type {!Array<settings.Account>}
     */
    accounts_: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * The targeted account for menu operations.
     * @private {?settings.Account}
     */
    actionMenuAccount_: Object,
  },

  /** @private {?settings.AccountManagerBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.addWebUIListener('accounts-changed', this.refreshAccounts_.bind(this));
  },

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.AccountManagerBrowserProxyImpl.getInstance();
    this.refreshAccounts_();
  },

  /**
   * @param {!settings.Route} newRoute
   * @param {settings.Route} oldRoute
   */
  currentRouteChanged: function(newRoute, oldRoute) {
    if (newRoute == settings.routes.ACCOUNT_MANAGER) {
      this.browserProxy_.showWelcomeDialogIfRequired();
    }
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
   * @param {!settings.Account} account
   * @return {boolean} True if the account reauthentication button should be
   *    shown, false otherwise.
   * @private
   */
  shouldShowReauthenticationButton_: function(account) {
    // Device account re-authentication cannot be handled in-session, primarily
    // because the user may have changed their password (leading to an LST
    // invalidation) and we do not have a mechanism to change the cryptohome
    // password in-session.
    return !account.isDeviceAccount && !account.isSignedIn;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onReauthenticationTap_: function(event) {
    this.browserProxy_.reauthenticateAccount(
      this.$['account-list'].itemForElement(event.path[0]).email);
  },

  /**
   * @private
   */
  refreshAccounts_: function() {
    this.browserProxy_.getAccounts().then(accounts => {
      this.set('accounts_', accounts);
    });
  },

  /**
   * Opens the Account actions menu.
   * @param {!{model: !{item: settings.Account}, target: !Element}} event
   * @private
   */
  onAccountActionsMenuButtonTap_: function(event) {
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
  onRemoveAccountTap_: function() {
    this.browserProxy_.removeAccount(
        /** @type {?settings.Account} */ (this.actionMenuAccount_));
    this.closeActionMenu_();
  }
});
