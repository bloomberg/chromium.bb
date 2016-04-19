// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.exportPath('settings');

/**
 * The state of sync. This is the data structure sent back and forth between
 * C++ and JS. Its naming and structure is not optimal, but changing it would
 * require changes to the C++ handler, which is already functional.
 * @typedef {{
 *   appsEnforced: boolean,
 *   appsRegistered: boolean,
 *   appsSynced: boolean,
 *   autofillEnforced: boolean,
 *   autofillRegistered: boolean,
 *   autofillSynced: boolean,
 *   bookmarksEnforced: boolean,
 *   bookmarksRegistered: boolean,
 *   bookmarksSynced: boolean,
 *   encryptAllData: boolean,
 *   encryptAllDataAllowed: boolean,
 *   enterGooglePassphraseBody: (string|undefined),
 *   enterPassphraseBody: (string|undefined),
 *   extensionsEnforced: boolean,
 *   extensionsRegistered: boolean,
 *   extensionsSynced: boolean,
 *   fullEncryptionBody: string,
 *   isGooglePassphrase: (boolean|undefined),
 *   passphrase: (string|undefined),
 *   passphraseFailed: boolean,
 *   passwordsEnforced: boolean,
 *   passwordsRegistered: boolean,
 *   passwordsSynced: boolean,
 *   preferencesEnforced: boolean,
 *   preferencesRegistered: boolean,
 *   preferencesSynced: boolean,
 *   showPassphrase: boolean,
 *   syncAllDataTypes: boolean,
 *   syncNothing: boolean,
 *   tabsEnforced: boolean,
 *   tabsRegistered: boolean,
 *   tabsSynced: boolean,
 *   themesEnforced: boolean,
 *   themesRegistered: boolean,
 *   themesSynced: boolean,
 *   typedUrlsEnforced: boolean,
 *   typedUrlsRegistered: boolean,
 *   typedUrlsSynced: boolean,
 *   usePassphrase: boolean,
 *   wifiCredentialsEnforced: (boolean|undefined),
 *   wifiCredentialsSynced: (boolean|undefined)
 * }}
 */
settings.SyncPrefs;

/**
 * @typedef {{actionLinkText: (string|undefined),
 *            childUser: (boolean|undefined),
 *            hasError: (boolean|undefined),
 *            hasUnrecoverableError: (boolean|undefined),
 *            managed: (boolean|undefined),
 *            setupCompleted: (boolean|undefined),
 *            setupInProgress: (boolean|undefined),
 *            signedIn: (boolean|undefined),
 *            signinAllowed: (boolean|undefined),
 *            signoutAllowed: (boolean|undefined),
 *            statusText: (string|undefined),
 *            supervisedUser: (boolean|undefined),
 *            syncSystemEnabled: (boolean|undefined)}}
 * @see chrome/browser/ui/webui/settings/sync_handler.cc
 */
settings.SyncStatus;

/**
 * @enum {string}
 */
settings.PageStatus = {
  SPINNER: 'spinner',                   // Before the page has loaded.
  CONFIGURE: 'configure',               // Preferences ready to be configured.
  TIMEOUT: 'timeout',                   // Preferences loading has timed out.
  DONE: 'done',                         // Sync subpage can be closed now.
  PASSPHRASE_ERROR: 'passphraseError',  // Error in the passphrase.
};

cr.define('settings', function() {
  /**
   * API which encapsulates messaging between JS and C++ for the sync page.
   * @constructor
   */
  function SyncPrivateApi() {}

  /** @private {?function(settings.SyncPrefs)} */
  SyncPrivateApi.syncPrefsCallback_ = null;

  /** @private {?function(settings.PageStatus)} */
  SyncPrivateApi.setPageStatusCallback_ = null;

  /**
   * Starts the signin process for the user. Does nothing if the user is
   * already signed in.
   * @private
   */
  SyncPrivateApi.startSignIn = function() {
    // TODO(tommycli): Currently this is always false, but this will become
    // a parameter once supervised users are implemented in MD Settings.
    var creatingSupervisedUser = false;
    chrome.send('SyncSetupStartSignIn', [creatingSupervisedUser]);
  };

  /**
   * Disconnects the signed in user.
   * @param {!boolean} deleteProfile
   * @private
   */
  SyncPrivateApi.disconnect = function(deleteProfile) {
    chrome.send('SyncSetupStopSyncing', [deleteProfile]);
  };

  /**
   * Determines the appropriate page to show in the Sync Setup UI based on
   * the state of the Sync backend. Does nothing if the user is not signed in.
   * @private
   */
  SyncPrivateApi.showSetupUI = function() {
    chrome.send('SyncSetupShowSetupUI');
    chrome.send('coreOptionsUserMetricsAction', ['Options_ShowSyncAdvanced']);
  };

  /**
   * Function to invoke when the sync page has been navigated to. This registers
   * the UI as the "active" sync UI so that if the user tries to open another
   * sync UI, this one will be shown instead.
   */
  SyncPrivateApi.didNavigateToSyncPage = function() {
    chrome.send('SyncSetupShowSetupUI');
  };

  /**
   * Function to invoke when leaving the sync page so that the C++ layer can be
   * notified that the sync UI is no longer open.
   */
  SyncPrivateApi.didNavigateAwayFromSyncPage = function() {
    SyncPrivateApi.setPageStatusCallback_ = null;
    chrome.send('SyncSetupDidClosePage');
  };

  /**
   * Sets the callback to be invoked when sync data has been fetched.
   * @param {!function(settings.SyncPrefs)} callback
   */
  SyncPrivateApi.setSyncPrefsCallback = function(callback) {
    SyncPrivateApi.syncPrefsCallback_ = callback;
  };

  /**
   * Handler for when state has been fetched from C++.
   * @param {!settings.SyncPrefs} syncPrefsFromCpp
   * @private
   */
  SyncPrivateApi.sendSyncPrefs_ = function(syncPrefsFromCpp) {
    if (SyncPrivateApi.syncPrefsCallback_)
      SyncPrivateApi.syncPrefsCallback_(syncPrefsFromCpp);
  };

  /**
   * Sets the sync state by sending it to the C++ layer.
   * @param {!settings.SyncPrefs} syncPrefs
   * @param {!function(settings.PageStatus)} callback
   */
  SyncPrivateApi.setSyncPrefs = function(syncPrefs, callback) {
    SyncPrivateApi.setPageStatusCallback_ = callback;
    chrome.send('SyncSetupConfigure', [JSON.stringify(syncPrefs)]);
  };

  /**
   * Handler for when setSyncPrefs() has either succeeded or failed.
   * @param {!settings.PageStatus} status
   * @private
   */
  SyncPrivateApi.setPageStatus_ = function(status) {
    if (SyncPrivateApi.setPageStatusCallback_)
      SyncPrivateApi.setPageStatusCallback_(status);

    SyncPrivateApi.setPageStatusCallback_ = null;
  };

  /**
   * Sends a request from JS to C++ for the current sync status.
   * @param {!function(settings.SyncStatus)} callback
   */
  SyncPrivateApi.getSyncStatus = function(callback) {
    SyncPrivateApi.syncStatusCallback_ = callback;
    chrome.send('SyncSetupGetSyncStatus');
  };

  /**
   * Handler for when sync status has been fetched from C++.
   * @param {!settings.SyncStatus} syncStatusFromCpp
   * @private
   */
  SyncPrivateApi.sendSyncStatus = function(syncStatusFromCpp) {
    if (SyncPrivateApi.syncStatusCallback_)
      SyncPrivateApi.syncStatusCallback_(syncStatusFromCpp);
  };

  /**
   * Sends a request from JS to C++ to open the multi-profile User Manager.
   */
  SyncPrivateApi.manageOtherPeople = function() {
    chrome.send('SyncSetupManageOtherPeople');
  };

  /**
   * This function encapsulates the logic that maps from the legacy
   * SyncSettingsHandler to an API natural to the new Polymer implementation.
   * @param {!settings.PageStatus} status
   * @param {!settings.SyncPrefs} prefs
   */
  SyncPrivateApi.showSyncSetupPage = function(status, prefs) {
    switch (status) {
      case settings.PageStatus.TIMEOUT:
      case settings.PageStatus.DONE:
        SyncPrivateApi.setPageStatus_(status);
        break;
      case settings.PageStatus.CONFIGURE:
        if (prefs.passphraseFailed) {
          SyncPrivateApi.setPageStatus_(
              settings.PageStatus.PASSPHRASE_ERROR);
          return;
        }

        SyncPrivateApi.sendSyncPrefs_(prefs);
        break;
      default:
        // Other statuses (i.e. "spinner") are ignored.
    }
  };

  return {
    SyncPrivateApi: SyncPrivateApi,
  };
});
