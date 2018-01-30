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
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /** @private {!settings.ProfileInfo} */
    profileInfo_: Object,
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached: function() {
    const profileInfoProxy = settings.ProfileInfoBrowserProxyImpl.getInstance();
    profileInfoProxy.getProfileInfo().then(this.handleProfileInfo_.bind(this));
    this.addWebUIListener(
        'profile-info-changed', this.handleProfileInfo_.bind(this));
    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /**
   * Handler for when the profile's icon and name is updated.
   * @private
   * @param {!settings.ProfileInfo} info
   */
  handleProfileInfo_: function(info) {
    this.profileInfo_ = info;
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    this.syncStatus = syncStatus;
  },

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectTap_: function() {
    /* This will route to people_page's disconnect dialog. */
    settings.navigateTo(settings.routes.SIGN_OUT);
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIcon_: function(iconUrl) {
    return cr.icon.getImage(iconUrl);
  },

  /**
   * @param {!settings.SyncStatus} syncStatus
   * @return {boolean} Whether to show the "Sign in to Chrome" button.
   * @private
   */
  showSignin_: function(syncStatus) {
    return !!syncStatus.signinAllowed && !syncStatus.signedIn;
  },
});