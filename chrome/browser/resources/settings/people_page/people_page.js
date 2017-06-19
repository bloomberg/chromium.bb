// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
Polymer({
  is: 'settings-people-page',

  behaviors: [
    settings.RouteObserverBehavior, I18nBehavior, WebUIListenerBehavior,
    // <if expr="chromeos">
    LockStateBehavior,
    // </if>
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * The current sync status, supplied by SyncBrowserProxy.
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

    /**
     * True if the current profile manages supervised users.
     */
    profileManagesSupervisedUsers_: Boolean,

    /**
     * The profile deletion warning. The message indicates the number of
     * profile stats that will be deleted if a non-zero count for the profile
     * stats is returned from the browser.
     */
    deleteProfileWarning_: String,

    /**
     * True if the profile deletion warning is visible.
     */
    deleteProfileWarningVisible_: Boolean,

    /**
     * True if the checkbox to delete the profile has been checked.
     */
    deleteProfile_: Boolean,

    // <if expr="not chromeos">
    /** @private */
    showImportDataDialog_: {
      type: Boolean,
      value: false,
    },
    // </if>

    /** @private */
    showDisconnectDialog_: Boolean,

    // <if expr="chromeos">
    /**
     * True if fingerprint settings should be displayed on this machine.
     * @private
     */
    fingerprintUnlockEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('fingerprintUnlockEnabled');
      },
      readOnly: true,
    },
    // </if>

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        map.set(settings.Route.SYNC.path, '#sync-status .subpage-arrow');
        // <if expr="not chromeos">
        map.set(
            settings.Route.MANAGE_PROFILE.path,
            '#picture-subpage-trigger .subpage-arrow');
        // </if>
        // <if expr="chromeos">
        map.set(
            settings.Route.CHANGE_PICTURE.path,
            '#picture-subpage-trigger .subpage-arrow');
        map.set(
            settings.Route.LOCK_SCREEN.path,
            '#lock-screen-subpage-trigger .subpage-arrow');
        map.set(
            settings.Route.ACCOUNTS.path,
            '#manage-other-people-subpage-trigger .subpage-arrow');
        // </if>
        return map;
      },
    },
  },

  /** @private {?settings.SyncBrowserProxy} */
  syncBrowserProxy_: null,

  /** @override */
  attached: function() {
    var profileInfoProxy = settings.ProfileInfoBrowserProxyImpl.getInstance();
    profileInfoProxy.getProfileInfo().then(this.handleProfileInfo_.bind(this));
    this.addWebUIListener(
        'profile-info-changed', this.handleProfileInfo_.bind(this));

    profileInfoProxy.getProfileManagesSupervisedUsers().then(
        this.handleProfileManagesSupervisedUsers_.bind(this));
    this.addWebUIListener(
        'profile-manages-supervised-users-changed',
        this.handleProfileManagesSupervisedUsers_.bind(this));

    this.addWebUIListener(
        'profile-stats-count-ready', this.handleProfileStatsCount_.bind(this));

    this.syncBrowserProxy_ = settings.SyncBrowserProxyImpl.getInstance();
    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  },

  /** @protected */
  currentRouteChanged: function() {
    this.showImportDataDialog_ =
        settings.getCurrentRoute() == settings.Route.IMPORT_DATA;

    if (settings.getCurrentRoute() == settings.Route.SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the disconnect dialog. There is another check when the sync status is
      // fetched. The dialog will be closed then the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        settings.navigateToPreviousRoute();
      } else {
        this.showDisconnectDialog_ = true;
        this.async(function() {
          this.$$('#disconnectDialog').showModal();
        }.bind(this));
      }
    } else if (this.showDisconnectDialog_) {
      this.$$('#disconnectDialog').close();
    }
  },

  // <if expr="chromeos">
  /** @private */
  getPasswordState_: function(hasPin, enableScreenLock) {
    if (!enableScreenLock)
      return this.i18n('lockScreenNone');
    if (hasPin)
      return this.i18n('lockScreenPinOrPassword');
    return this.i18n('lockScreenPasswordOnly');
  },
  // </if>

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
   * Handler for when the profile starts or stops managing supervised users.
   * @private
   * @param {boolean} managesSupervisedUsers
   */
  handleProfileManagesSupervisedUsers_: function(managesSupervisedUsers) {
    this.profileManagesSupervisedUsers_ = managesSupervisedUsers;
  },

  /**
   * Handler for when the profile stats count is pushed from the browser.
   * @param {number} count
   * @private
   */
  handleProfileStatsCount_: function(count) {
    this.deleteProfileWarning_ = (count > 0) ?
        (count == 1) ? loadTimeData.getStringF(
                           'deleteProfileWarningWithCountsSingular',
                           this.syncStatus.signedInUsername) :
                       loadTimeData.getStringF(
                           'deleteProfileWarningWithCountsPlural', count,
                           this.syncStatus.signedInUsername) :
        loadTimeData.getStringF(
            'deleteProfileWarningWithoutCounts',
            this.syncStatus.signedInUsername);
  },

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?settings.SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_: function(syncStatus) {
    if (!this.syncStatus && syncStatus && !syncStatus.signedIn)
      chrome.metricsPrivate.recordUserAction('Signin_Impression_FromSettings');

    // <if expr="not chromeos">
    if (syncStatus.signedIn)
      settings.ProfileInfoBrowserProxyImpl.getInstance().getProfileStatsCount();
    // </if>

    if (!syncStatus.signedIn && this.showDisconnectDialog_)
      this.$$('#disconnectDialog').close();

    this.syncStatus = syncStatus;
  },

  /** @private */
  onPictureTap_: function() {
    // <if expr="chromeos">
    settings.navigateTo(settings.Route.CHANGE_PICTURE);
    // </if>
    // <if expr="not chromeos">
    settings.navigateTo(settings.Route.MANAGE_PROFILE);
    // </if>
  },

  // <if expr="not chromeos">
  /** @private */
  onProfileNameTap_: function() {
    settings.navigateTo(settings.Route.MANAGE_PROFILE);
  },
  // </if>

  /** @private */
  onSigninTap_: function() {
    this.syncBrowserProxy_.startSignIn();
  },

  /** @private */
  onDisconnectClosed_: function() {
    this.showDisconnectDialog_ = false;
    cr.ui.focusWithoutInk(assert(this.$$('#disconnectButton')));

    if (settings.getCurrentRoute() == settings.Route.SIGN_OUT)
      settings.navigateToPreviousRoute();
    this.fire('signout-dialog-closed');
  },

  /** @private */
  onDisconnectTap_: function() {
    settings.navigateTo(settings.Route.SIGN_OUT);
  },

  /** @private */
  onDisconnectCancel_: function() {
    this.$$('#disconnectDialog').close();
  },

  /** @private */
  onDisconnectConfirm_: function() {
    var deleteProfile = !!this.syncStatus.domain || this.deleteProfile_;
    // Trigger the sign out event after the navigateToPreviousRoute().
    // So that the navigation to the setting page could be finished before the
    // sign out if navigateToPreviousRoute() returns synchronously even the
    // browser is closed after the sign out. Otherwise, the navigation will be
    // finshed during session restore if the browser is closed before the async
    // callback executed.
    listenOnce(this, 'signout-dialog-closed', function() {
      this.syncBrowserProxy_.signOut(deleteProfile);
    }.bind(this));

    this.$$('#disconnectDialog').close();
  },

  /** @private */
  onSyncTap_: function() {
    assert(this.syncStatus.signedIn);
    assert(this.syncStatus.syncSystemEnabled);

    if (!this.isSyncStatusActionable_(this.syncStatus))
      return;

    switch (this.syncStatus.statusAction) {
      case settings.StatusAction.REAUTHENTICATE:
        this.syncBrowserProxy_.startSignIn();
        break;
      case settings.StatusAction.SIGNOUT_AND_SIGNIN:
        // <if expr="chromeos">
        this.syncBrowserProxy_.attemptUserExit();
        // </if>
        // <if expr="not chromeos">
        if (this.syncStatus.domain)
          settings.navigateTo(settings.Route.SIGN_OUT);
        else {
          // Silently sign the user out without deleting their profile and
          // prompt them to sign back in.
          this.syncBrowserProxy_.signOut(false);
          this.syncBrowserProxy_.startSignIn();
        }
        // </if>
        break;
      case settings.StatusAction.UPGRADE_CLIENT:
        settings.navigateTo(settings.Route.ABOUT);
        break;
      case settings.StatusAction.ENTER_PASSPHRASE:
      case settings.StatusAction.CONFIRM_SYNC_SETTINGS:
      case settings.StatusAction.NO_ACTION:
      default:
        settings.navigateTo(settings.Route.SYNC);
    }
  },

  // <if expr="chromeos">
  /**
   * @param {!Event} e
   * @private
   */
  onConfigureLockTap_: function(e) {
    // Navigating to the lock screen will always open the password prompt
    // dialog, so prevent the end of the tap event to focus what is underneath
    // it, which takes focus from the dialog.
    e.preventDefault();
    settings.navigateTo(settings.Route.LOCK_SCREEN);
  },
  // </if>

  /** @private */
  onManageOtherPeople_: function() {
    // <if expr="not chromeos">
    this.syncBrowserProxy_.manageOtherPeople();
    // </if>
    // <if expr="chromeos">
    settings.navigateTo(settings.Route.ACCOUNTS);
    // </if>
  },

  // <if expr="not chromeos">
  /**
   * @private
   * @param {string} domain
   * @return {string}
   */
  getDomainHtml_: function(domain) {
    var innerSpan = '<span id="managed-by-domain-name">' + domain + '</span>';
    return loadTimeData.getStringF('domainManagedProfile', innerSpan);
  },

  /** @private */
  onImportDataTap_: function() {
    settings.navigateTo(settings.Route.IMPORT_DATA);
  },

  /** @private */
  onImportDataDialogClosed_: function() {
    settings.navigateToPreviousRoute();
    cr.ui.focusWithoutInk(assert(this.$.importDataDialogTrigger));
  },
  // </if>

  /**
   * @private
   * @param {string} domain
   * @return {string}
   */
  getDisconnectExplanationHtml_: function(domain) {
    // <if expr="not chromeos">
    if (domain) {
      return loadTimeData.getStringF(
          'syncDisconnectManagedProfileExplanation',
          '<span id="managed-by-domain-name">' + domain + '</span>');
    }
    // </if>
    return loadTimeData.getString('syncDisconnectExplanation');
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean}
   */
  isAdvancedSyncSettingsVisible_: function(syncStatus) {
    return !!syncStatus && !!syncStatus.signedIn &&
        !!syncStatus.syncSystemEnabled;
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {boolean} Whether an action can be taken with the sync status. sync
   *     status is actionable if sync is not managed and if there is a sync
   *     error, there is an action associated with it.
   */
  isSyncStatusActionable_: function(syncStatus) {
    return !!syncStatus && !syncStatus.managed &&
        (!syncStatus.hasError ||
         syncStatus.statusAction != settings.StatusAction.NO_ACTION);
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {string}
   */
  getSyncIcon_: function(syncStatus) {
    if (!syncStatus)
      return '';

    var syncIcon = 'settings:sync';

    if (syncStatus.hasError)
      syncIcon = 'settings:sync-problem';

    // Override the icon to the disabled icon if sync is managed.
    if (syncStatus.managed)
      syncIcon = 'settings:sync-disabled';

    return syncIcon;
  },

  /**
   * @private
   * @param {?settings.SyncStatus} syncStatus
   * @return {string} The class name for the sync status text.
   */
  getSyncStatusTextClass_: function(syncStatus) {
    return (!!syncStatus && syncStatus.hasError) ? 'sync-error' : '';
  },

  /**
   * @param {string} iconUrl
   * @return {string} A CSS imageset for multiple scale factors.
   * @private
   */
  getIconImageset_: function(iconUrl) {
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
