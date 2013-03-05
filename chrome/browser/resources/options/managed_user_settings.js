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

      $('lock-settings').onclick = function() {
        chrome.send('setElevated', [false]);
        // The managed user is currently authenticated, so don't wait for a
        // callback to set the new authentication state since a reset to not
        // elevated is done without showing the passphrase dialog.
        self.authenticationState = ManagedUserAuthentication.UNAUTHENTICATED;
        self.enableControls(false);
      };

      $('unlock-settings').onclick = function() {
        if (self.authenticationState == ManagedUserAuthentication.CHECKING)
          return;
        self.authenticationState = ManagedUserAuthentication.CHECKING;
        chrome.send('setElevated', [true]);
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

    /** Update the page according to the current authentication state */
    didShowPage: function() {
      var isAuthenticated =
          this.authenticationState == ManagedUserAuthentication.AUTHENTICATED;
      this.enableControls(isAuthenticated);
    },

    // Enables or disables all controls based on the authentication state of
    // the managed user. If |enable| is true, the controls will be enabled.
    enableControls: function(enable) {
      $('set-passphrase').disabled =
          !enable || !$('use-passphrase-checkbox').checked;
      // TODO(sergiu): make $('get-content-packs-button') behave the same as
      // the other controls once the button actually does something.
      $('contentpacks-allow').setDisabled('notManagedUserModifiable', !enable);
      $('contentpacks-warn').setDisabled('notManagedUserModifiable', !enable);
      $('contentpacks-block').setDisabled('notManagedUserModifiable', !enable);
      $('safe-search-checkbox').setDisabled(
          'notManagedUserModifiable', !enable);
      // TODO(akuegel): Add disable-signin-checkbox and
      // disable-history-deletion-checkbox once these features are implemented
      $('use-passphrase-checkbox').disabled = !enable;
      if (enable)
        $('managed-user-settings-page').classList.remove('locked');
      else
        $('managed-user-settings-page').classList.add('locked');
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
      chrome.send('setElevated', [false]);
    },
  };

  // Sets the authentication state of the managed user. |success| is true if
  // the authentication was successful.
  ManagedUserSettings.isAuthenticated = function(success) {
    ManagedUserSettings.getInstance().isAuthenticated_(success);
  };

  // Sets the use passphrase checkbox according to if a passphrase is specified
  // or not. |hasPassphrase| is true if the local passphrase is non-empty.
  ManagedUserSettings.passphraseChanged = function(hasPassphrase) {
    var instance = ManagedUserSettings.getInstance();
    if (instance.authenticationState == ManagedUserAuthentication.AUTHENTICATED)
      $('set-passphrase').disabled = !hasPassphrase;
    $('use-passphrase-checkbox').checked = hasPassphrase;
    ManagedUserSettings.getInstance().isPassphraseSet = hasPassphrase;
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
