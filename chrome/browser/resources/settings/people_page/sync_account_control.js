// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-sync-account-section' is the settings page containing sign-in
 * settings.
 */
Polymer({
  is: 'settings-sync-account-control',
  behaviors: [WebUIListenerBehavior],
  properties: {
    /**
     * The current sync status, supplied by SyncBrowserProxy.
     * @type {!settings.SyncStatus}
     */
    syncStatus: Object,

    /** @private {!Array<!settings.StoredAccount>} */
    storedAccounts_: Object,

    /** @private {?settings.StoredAccount} */
    shownAccount_: Object,

    promoLabel: String,

    promoSecondaryLabel: String,

    /** @private {boolean} */
    shouldShowAvatarRow_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowAvatarRow_(storedAccounts_, syncStatus,' +
          'storedAccounts_.length, syncStatus.signedIn)',
      observer: 'onShouldShowAvatarRowChange_',
    }
  },

  observers: [
    'onShownAccountShouldChange_(storedAccounts_, syncStatus)',
  ],

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
  },

  /**
   * @param {string} label
   * @param {string} name
   * @return {string}
   * @private
   */
  getSubstituteLabel_: function(label, name) {
    return loadTimeData.substituteString(label, name);
  },

  /**
   * @param {string} label
   * @param {string} name
   * @return {string}
   * @private
   */
  getNameDisplay_: function(label, name) {
    return this.syncStatus.signedIn ?
        loadTimeData.substituteString(label, name) :
        name;
  },

  /**
   * @param {?string} image
   * @return {string}
   * @private
   */
  getAccountImageSrc_: function(image) {
    // image can be undefined if the account has not set an avatar photo.
    return image || 'chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE';
  },

  /**
   * @param {!Array<!settings.StoredAccount>} accounts
   * @private
   */
  handleStoredAccounts_: function(accounts) {
    this.storedAccounts_ = accounts;
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {!settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowAvatarRow_: function() {
    return this.syncStatus.signedIn || this.storedAccounts_.length > 0;
  },

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();

    // Need to close here since one menu item also triggers this function.
    if (this.$$('#menu')) {
      /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
    }
  },

  /** @private */
  onSyncButtonTap_: function() {
    assert(this.shownAccount_);
    this.syncBrowserProxy_.startSyncingWithEmail(this.shownAccount_.email);
  },

  /** @private */
  onTurnOffButtonTap_: function() {
    /* This will route to people_page's disconnect dialog. */
    settings.navigateTo(settings.routes.SIGN_OUT);
  },

  /** @private */
  onMenuButtonTap_: function() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('#menu'));
    actionMenu.showAt(assert(this.$$('#dots')), {
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  },

  /** @private */
  onShouldShowAvatarRowChange_: function() {
    // Close dropdown when avatar-row hides, so if it appears again, the menu
    // won't be open by default.
    const actionMenu = this.$$('#menu');
    if (!this.shouldShowAvatarRow_ && actionMenu && actionMenu.open)
      actionMenu.close();
  },

  /**
   * @param {!{model:
   *          !{item: !settings.StoredAccount},
   *        }} e
   * @private
   */
  onAccountTap_: function(e) {
    this.shownAccount_ = e.model.item;
    /** @type {!CrActionMenuElement} */ (this.$$('#menu')).close();
  },

  /** @private */
  onShownAccountShouldChange_: function() {
    if (this.syncStatus.signedIn) {
      for (let i = 0; i < this.storedAccounts_.length; i++) {
        if (this.storedAccounts_[i].email == this.syncStatus.signedInUsername) {
          this.shownAccount_ = this.storedAccounts_[i];
          return;
        }
      }
    } else {
      this.shownAccount_ =
          this.storedAccounts_ ? this.storedAccounts_[0] : null;
    }
  }
});