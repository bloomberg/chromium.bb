// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (loadTimeData.getBoolean('managedUsersEnabled')) {

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var SettingsDialog = options.SettingsDialog;

  //////////////////////////////////////////////////////////////////////////////
  // ManagedUserSettings class:

  /**
   * Encapsulated handling of the Managed User Settings page.
   * @constructor
   * @class
   */
  function ManagedUserSettings() {
    SettingsDialog.call(
        this,
        'manageduser',
        loadTimeData.getString('managedUserSettingsPageTabTitle'),
        'managed-user-settings-page',
        $('managed-user-settings-confirm'),
        $('managed-user-settings-cancel'));
  }

  cr.addSingletonGetter(ManagedUserSettings);

  // Represents the three possible authentication states.
  var ManagedUserAuthentication = {
    // The manager of the managed account is not authenticated.
    UNAUTHENTICATED: 'unauthenticated',

    // The authentication is currently being checked.
    CHECKING: 'checking',

    // The manager of the managed account is authenticated.
    AUTHENTICATED: 'authenticated'
  };

  ManagedUserSettings.prototype = {
    // Inherit from SettingsDialog.
    __proto__: SettingsDialog.prototype,

    // The current authentication state of the manager of the managed account.
    authenticationState: ManagedUserAuthentication.UNAUTHENTICATED,

    // True if the local passphrase of the manager of the managed account is
    // set. If it is not set, no authentication is required.
    isPassphraseSet: false,

    /**
     * Initialize the page.
     * @override
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      SettingsDialog.prototype.initializePage.call(this);

      $('get-content-packs-button').onclick = function(event) {
        window.open(loadTimeData.getString('getContentPacksURL'));
      };

      $('set-passphrase').onclick = function() {
        OptionsPage.navigateToPage('setPassphrase');
      };

      $('use-passphrase-checkbox').onclick = function() {
        $('set-passphrase').disabled = !$('use-passphrase-checkbox').checked;
      };

      var self = this;
      $('unlock-settings').onclick = function() {
        if (self.authenticationState == ManagedUserAuthentication.CHECKING)
          return;
        chrome.send('displayPassphraseDialog',
                    ['options.ManagedUserSettings.isAuthenticated']);
        self.authenticationState = ManagedUserAuthentication.CHECKING;
      };
    },

    /** @override */
    handleConfirm: function() {
      if ($('use-passphrase-checkbox').checked && !this.isPassphraseSet) {
        OptionsPage.navigateToPage('setPassphrase');
        return;
      }
      if (!$('use-passphrase-checkbox').checked)
        chrome.send('resetPassphrase');
      chrome.send('confirmManagedUserSettings');
      SettingsDialog.prototype.handleConfirm.call(this);
    },

    // Enables or disables all controls based on the authentication state of
    // the managed user. If |enable| is true, the controls will be enabled.
    enableControls: function(enable) {
      $('set-passphrase').disabled = !enable;
      $('get-content-packs-button').disabled = !enable;
      $('contentpacks-allow').setDisabled('notManagedUserModifiable', !enable);
      $('contentpacks-warn').setDisabled('notManagedUserModifiable', !enable);
      $('contentpacks-block').setDisabled('notManagedUserModifiable', !enable);
      $('safe-search-checkbox').setDisabled(
          'notManagedUserModifiable', !enable);
      // TODO(akuegel): Add disable-signin-checkbox and
      // disable-history-deletion-checkbox once these features are implemented
      $('use-passphrase-checkbox').disabled = !enable;
      $('unlock-settings').disabled = enable;
    },

    // Is called when the passphrase dialog is closed. |success| is true
    // if the authentication was successful.
    isAuthenticated_: function(success) {
      if (success) {
        this.authenticationState = ManagedUserAuthentication.AUTHENTICATED;
        this.enableControls(true);
      } else {
        this.authenticationState = ManagedUserAuthentication.UNAUTHENTICATED;
      }
    },

    // Reset the authentication to UNAUTHENTICATED when the managed user
    // settings dialog is closed.
    didClosePage: function() {
      // Reset the authentication of the custodian.
      this.authenticationState = ManagedUserAuthentication.UNAUTHENTICATED;
      chrome.send('endAuthentication');
    },
  };

  // Is called when the page is initialized. |hasPassphrase| is true if there
  // is a local passphrase required to unlock the controls of the page.
  // Also, the "use passphrase" checkbox will be initialized accordingly.
  ManagedUserSettings.initializeSetPassphraseButton = function(hasPassphrase) {
    $('set-passphrase').disabled = !hasPassphrase;
    $('use-passphrase-checkbox').checked = hasPassphrase;
    var instance = ManagedUserSettings.getInstance();
    instance.isPassphraseSet = hasPassphrase;
    if (hasPassphrase) {
      instance.authenticationState = ManagedUserAuthentication.UNAUTHENTICATED;
      instance.enableControls(false);
    } else {
      instance.authenticationState = ManagedUserAuthentication.AUTHENTICATED;
      $('unlock-settings').disabled = true;
    }
  };

  // Is called when the passphrase dialog is closed. |success| is true
  // if the authentication was successful.
  ManagedUserSettings.isAuthenticated = function(success) {
    ManagedUserSettings.getInstance().isAuthenticated_(success);
  };

  // Is called whenever the local passphrase has changed. |isPassphraseSet|
  // is true if the local passphrase is non-empty.
  ManagedUserSettings.passphraseChanged = function(isPassphraseSet) {
    ManagedUserSettings.getInstance().isPassphraseSet = isPassphraseSet;
  };

  var ManagedUserSettingsForTesting = {
    getSetPassphraseButton: function() {
      return $('set-passphrase');
    },
    getUnlockButton: function() {
      return $('unlock-settings');
    }
  };

  // Export
  return {
    ManagedUserSettings: ManagedUserSettings,
    ManagedUserSettingsForTesting: ManagedUserSettingsForTesting,
    ManagedUserAuthentication: ManagedUserAuthentication
  };
});

}
