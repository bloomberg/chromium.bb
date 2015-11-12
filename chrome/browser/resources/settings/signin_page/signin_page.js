// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-signin-page' is the settings page containing sign-in settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-signin-page prefs="{{prefs}}"></settings-signin-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-signin-page
 */
Polymer({
  is: 'settings-signin-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by settings.SyncPrivateApi.
     * @type {?settings.SyncPrivateApi.SyncStatus}
     */
    syncStatus: Object,
  },

  created: function() {
    settings.SyncPrivateApi.getSyncStatus(
        this.handleSyncStatusFetched_.bind(this));
  },

  /**
   * Handler for when the sync state is pushed from settings.SyncPrivateApi.
   * @private
   */
  handleSyncStatusFetched_: function(syncStatus) {
    this.syncStatus = syncStatus;

    // TODO(tommycli): Remove once we figure out how to refactor the sync
    // code to not include HTML in the status messages.
    this.$.syncStatusText.innerHTML = syncStatus.statusText;
  },

  /** @private */
  onActionLinkTap_: function() {
    settings.SyncPrivateApi.showSetupUI();
  },

  /** @private */
  onSigninTap_: function() {
    settings.SyncPrivateApi.startSignIn();
  },

  /** @private */
  onDisconnectTap_: function() {
    this.$.disconnectDialog.open();
  },

  /** @private */
  onDisconnectConfirm_: function() {
    var deleteProfile = this.$.deleteProfile && this.$.deleteProfile.checked;
    settings.SyncPrivateApi.disconnect(deleteProfile);

    // Dialog automatically closed because button has dialog-confirm attribute.
  },

  /** @private */
  onSyncTap_: function() {
    this.$.pages.setSubpageChain(['sync']);
  },

  /**
   * @private
   * @return {boolean}
   */
  isStatusTextSet_: function() {
    return this.syncStatus && this.syncStatus.statusText.length > 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  isAdvancedSyncSettingsVisible_: function() {
    var status = this.syncStatus;
    return status && status.signedIn && !status.managed &&
           status.syncSystemEnabled;
  },
});
