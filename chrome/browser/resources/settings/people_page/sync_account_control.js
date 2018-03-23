// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'settings-sync-account-section' is the settings page containing sign-in
 * settings.
 */
cr.exportPath('settings');

/** @const {number} */
settings.MAX_SIGNIN_PROMO_IMPRESSION = 10;

Polymer({
  is: 'settings-sync-account-control',
  behaviors: [WebUIListenerBehavior],
  properties: {
    /**
     * The current sync status, supplied by parent element.
     * @type {!settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * Proxy variable for syncStatus.signedIn to shield observer from being
     * triggered multiple times whenever syncStatus changes.
     * @private {boolean}
     */
    signedIn_: {
      type: Boolean,
      computed: 'computeSignedIn_(syncStatus.signedIn)',
      observer: 'onSignedInChanged_',
    },

    /** @private {!Array<!settings.StoredAccount>} */
    storedAccounts_: Object,

    /** @private {?settings.StoredAccount} */
    shownAccount_: Object,

    showingPromo: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

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

  created: function() {
    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.syncBrowserProxy_.getStoredAccounts().then(
        this.handleStoredAccounts_.bind(this));
    this.addWebUIListener(
        'stored-accounts-updated', this.handleStoredAccounts_.bind(this));
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_: function() {
    return !!this.syncStatus.signedIn;
  },

  /** @private */
  onSignedInChanged_: function() {
    if (!this.showingPromo && !this.syncStatus.signedIn &&
        this.syncBrowserProxy_.getPromoImpressionCount() <
            settings.MAX_SIGNIN_PROMO_IMPRESSION) {
      this.showingPromo = true;
      this.syncBrowserProxy_.incrementPromoImpressionCount();
    } else {
      // Turn off the promo if the user is signed in.
      this.showingPromo = false;
    }
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
  getNameDisplay_: function(label, name, errorLabel) {
    if (this.syncStatus.hasError === true)
      return errorLabel;

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
   * @return {string}
   * @private
   */
  getSyncIcon_: function() {
    return this.syncStatus.hasError ? 'settings:sync-problem' : 'settings:sync';
  },

  /**
   * @param {!Array<!settings.StoredAccount>} accounts
   * @private
   */
  handleStoredAccounts_: function(accounts) {
    this.storedAccounts_ = accounts;
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
    actionMenu.showAt(assert(this.$$('#dropdown-arrow')));
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