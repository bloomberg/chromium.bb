// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the the People section to get the
 * status of the sync backend and user preferences on what data to sync. Used
 * for both Chrome browser and ChromeOS.
 */
cr.exportPath('settings');

/**
 * @typedef {{childUser: (boolean|undefined),
 *            domain: (string|undefined),
 *            hasError: (boolean|undefined),
 *            hasUnrecoverableError: (boolean|undefined),
 *            managed: (boolean|undefined),
 *            setupCompleted: (boolean|undefined),
 *            setupInProgress: (boolean|undefined),
 *            signedIn: (boolean|undefined),
 *            signedInUsername: (string|undefined),
 *            signinAllowed: (boolean|undefined),
 *            statusAction: (!settings.StatusAction),
 *            statusText: (string|undefined),
 *            supervisedUser: (boolean|undefined),
 *            syncSystemEnabled: (boolean|undefined)}}
 * @see chrome/browser/ui/webui/settings/people_handler.cc
 */
settings.SyncStatus;


/**
 * Must be kept in sync with the return values of getSyncErrorAction in
 * chrome/browser/ui/webui/settings/people_handler.cc
 * @enum {string}
 */
settings.StatusAction = {
  NO_ACTION: 'noAction',             // No action to take.
  REAUTHENTICATE: 'reauthenticate',  // User needs to reauthenticate.
  SIGNOUT_AND_SIGNIN:
      'signOutAndSignIn',               // User needs to sign out and sign in.
  UPGRADE_CLIENT: 'upgradeClient',      // User needs to upgrade the client.
  ENTER_PASSPHRASE: 'enterPassphrase',  // User needs to enter passphrase.
  CONFIRM_SYNC_SETTINGS:
      'confirmSyncSettings',  // User needs to confirm sync settings.
};

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
 *   passphrase: (string|undefined),
 *   passphraseRequired: boolean,
 *   passphraseTypeIsCustom: boolean,
 *   passwordsEnforced: boolean,
 *   passwordsRegistered: boolean,
 *   passwordsSynced: boolean,
 *   paymentsIntegrationEnabled: boolean,
 *   preferencesEnforced: boolean,
 *   preferencesRegistered: boolean,
 *   preferencesSynced: boolean,
 *   setNewPassphrase: (boolean|undefined),
 *   syncAllDataTypes: boolean,
 *   tabsEnforced: boolean,
 *   tabsRegistered: boolean,
 *   tabsSynced: boolean,
 *   themesEnforced: boolean,
 *   themesRegistered: boolean,
 *   themesSynced: boolean,
 *   typedUrlsEnforced: boolean,
 *   typedUrlsRegistered: boolean,
 *   typedUrlsSynced: boolean,
 * }}
 */
settings.SyncPrefs;

/**
 * @enum {string}
 */
settings.PageStatus = {
  SPINNER: 'spinner',                     // Before the page has loaded.
  CONFIGURE: 'configure',                 // Preferences ready to be configured.
  TIMEOUT: 'timeout',                     // Preferences loading has timed out.
  DONE: 'done',                           // Sync subpage can be closed now.
  PASSPHRASE_FAILED: 'passphraseFailed',  // Error in the passphrase.
};

cr.define('settings', function() {
  /** @interface */
  class SyncBrowserProxy {
    // <if expr="not chromeos">
    /**
     * Starts the signin process for the user. Does nothing if the user is
     * already signed in.
     */
    startSignIn() {}

    /**
     * Signs out the signed-in user.
     * @param {boolean} deleteProfile
     */
    signOut(deleteProfile) {}

    /**
     * Opens the multi-profile user manager.
     */
    manageOtherPeople() {}

    // </if>

    // <if expr="chromeos">
    /**
     * Signs the user out.
     */
    attemptUserExit() {}

    // </if>

    /**
     * Gets the current sync status.
     * @return {!Promise<!settings.SyncStatus>}
     */
    getSyncStatus() {}

    /**
     * Function to invoke when the sync page has been navigated to. This
     * registers the UI as the "active" sync UI so that if the user tries to
     * open another sync UI, this one will be shown instead.
     */
    didNavigateToSyncPage() {}

    /**
     * Function to invoke when leaving the sync page so that the C++ layer can
     * be notified that the sync UI is no longer open.
     */
    didNavigateAwayFromSyncPage() {}

    /**
     * Sets which types of data to sync.
     * @param {!settings.SyncPrefs} syncPrefs
     * @return {!Promise<!settings.PageStatus>}
     */
    setSyncDatatypes(syncPrefs) {}

    /**
     * Sets the sync encryption options.
     * @param {!settings.SyncPrefs} syncPrefs
     * @return {!Promise<!settings.PageStatus>}
     */
    setSyncEncryption(syncPrefs) {}

    /**
     * Opens the Google Activity Controls url in a new tab.
     */
    openActivityControlsUrl() {}
  }

  /**
   * @implements {settings.SyncBrowserProxy}
   */
  class SyncBrowserProxyImpl {
    // <if expr="not chromeos">
    /** @override */
    startSignIn() {
      chrome.send('SyncSetupStartSignIn');
    }

    /** @override */
    signOut(deleteProfile) {
      chrome.send('SyncSetupStopSyncing', [deleteProfile]);
    }

    /** @override */
    manageOtherPeople() {
      chrome.send('SyncSetupManageOtherPeople');
    }

    // </if>
    // <if expr="chromeos">
    /** @override */
    attemptUserExit() {
      return chrome.send('AttemptUserExit');
    }

    // </if>

    /** @override */
    getSyncStatus() {
      return cr.sendWithPromise('SyncSetupGetSyncStatus');
    }

    /** @override */
    didNavigateToSyncPage() {
      chrome.send('SyncSetupShowSetupUI');
    }

    /** @override */
    didNavigateAwayFromSyncPage() {
      chrome.send('SyncSetupDidClosePage');
    }

    /** @override */
    setSyncDatatypes(syncPrefs) {
      return cr.sendWithPromise(
          'SyncSetupSetDatatypes', JSON.stringify(syncPrefs));
    }

    /** @override */
    setSyncEncryption(syncPrefs) {
      return cr.sendWithPromise(
          'SyncSetupSetEncryption', JSON.stringify(syncPrefs));
    }

    /** @override */
    openActivityControlsUrl() {
      chrome.metricsPrivate.recordUserAction(
          'Signin_AccountSettings_GoogleActivityControlsClicked');
    }
  }

  cr.addSingletonGetter(SyncBrowserProxyImpl);

  return {
    SyncBrowserProxy: SyncBrowserProxy,
    SyncBrowserProxyImpl: SyncBrowserProxyImpl,
  };
});
