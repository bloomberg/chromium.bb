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

  ManagedUserSettings.prototype = {
    // Inherit from SettingsDialog.
    __proto__: SettingsDialog.prototype,

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

    },
    /** @override */
    handleConfirm: function() {
      chrome.send('confirmManagedUserSettings');
      SettingsDialog.prototype.handleConfirm.call(this);
    },
  };

  var ManagedUserSettingsForTesting = {
    getSetPassphraseButton: function() {
      return $('set-passphrase');
    }
  };

   // Export
  return {
    ManagedUserSettings: ManagedUserSettings,
    ManagedUserSettingsForTesting: ManagedUserSettingsForTesting
  };
});

}
