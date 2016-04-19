// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-people-page prefs="{{prefs}}"></settings-people-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-people-page',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
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
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by settings.SyncPrivateApi.
     * @type {?settings.SyncStatus}
     */
    syncStatus: Object,

    /**
     * The currently selected profile icon URL. May be a data URL.
     */
    profileIconUrl_: String,

    /**
     * The current profile name.
     */
    profileName_: String,

<if expr="chromeos">
    /** @private {!settings.EasyUnlockBrowserProxyImpl} */
    easyUnlockBrowserProxy_: {
      type: Object,
      value: function() {
        return settings.EasyUnlockBrowserProxyImpl.getInstance();
      },
    },

    /**
     * True if Easy Unlock is allowed on this machine.
     */
    easyUnlockAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockAllowed');
      },
      readOnly: true,
    },

    /**
     * True if Easy Unlock is enabled.
     */
    easyUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockEnabled');
      },
    },

    /**
     * True if Easy Unlock's proximity detection feature is allowed.
     */
    easyUnlockProximityDetectionAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('easyUnlockAllowed') &&
            loadTimeData.getBoolean('easyUnlockProximityDetectionAllowed');
      },
      readOnly: true,
    },
</if>
  },

  /** @override */
  attached: function() {
    settings.ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
        this.handleProfileInfo_.bind(this));
    this.addWebUIListener('profile-info-changed',
                          this.handleProfileInfo_.bind(this));

    settings.SyncPrivateApi.getSyncStatus(
        this.handleSyncStatusFetched_.bind(this));

<if expr="chromeos">
    if (this.easyUnlockAllowed_) {
      this.addWebUIListener(
          'easy-unlock-enabled-status',
          this.handleEasyUnlockEnabledStatusChanged_.bind(this));
      this.easyUnlockBrowserProxy_.getEnabledStatus().then(
          this.handleEasyUnlockEnabledStatusChanged_.bind(this));
    }
</if>
  },

  /**
   * Handler for when the profile's icon and name is updated.
   * @private
   * @param {!settings.ProfileInfo} info
   */
  handleProfileInfo_: function(info) {
    this.profileName_ = info.name;
    this.profileIconUrl_ = info.iconUrl;
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

<if expr="chromeos">
  /**
   * Handler for when the Easy Unlock enabled status has changed.
   * @private
   */
  handleEasyUnlockEnabledStatusChanged_: function(easyUnlockEnabled) {
    this.easyUnlockEnabled_ = easyUnlockEnabled;
  },
</if>

  /** @private */
  onActionLinkTap_: function() {
    settings.SyncPrivateApi.showSetupUI();
  },

  /** @private */
  onPictureTap_: function() {
<if expr="chromeos">
    this.$.pages.setSubpageChain(['changePicture']);
</if>
<if expr="not chromeos">
    this.$.pages.setSubpageChain(['manageProfile']);
</if>
  },

<if expr="not chromeos">
  /** @private */
  onProfileNameTap_: function() {
    this.$.pages.setSubpageChain(['manageProfile']);
  },
</if>

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

<if expr="chromeos">
  /** @private */
  onEasyUnlockSetupTap_: function() {
    this.easyUnlockBrowserProxy_.startTurnOnFlow();
  },

  /** @private */
  onEasyUnlockTurnOffTap_: function() {
    this.$$('#easyUnlockTurnOffDialog').open();
  },
</if>

  /** @private */
  onManageOtherPeople_: function() {
<if expr="not chromeos">
    settings.SyncPrivateApi.manageOtherPeople();
</if>
<if expr="chromeos">
    this.$.pages.setSubpageChain(['users']);
</if>
  },

  /**
   * @private
   * @return {boolean}
   */
  isStatusTextSet_: function(syncStatus) {
    return syncStatus && syncStatus.statusText.length > 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  isAdvancedSyncSettingsVisible_: function(syncStatus) {
    return syncStatus && syncStatus.signedIn && !syncStatus.managed &&
           syncStatus.syncSystemEnabled;
  },
});
