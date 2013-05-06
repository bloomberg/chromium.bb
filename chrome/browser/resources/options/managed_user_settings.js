// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (loadTimeData.getBoolean('managedUsersEnabled')) {

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  //////////////////////////////////////////////////////////////////////////////
  // ManagedUserSettings class:

  /**
   * Encapsulated handling of the Managed User Settings page.
   * @constructor
   * @class
   */
  function ManagedUserSettings() {
    OptionsPage.call(
        this,
        'manageduser',
        loadTimeData.getString('managedUserSettingsPageTabTitle'),
        'managed-user-settings-page');
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

  /**
   * Matches ManagedModeURLFilter::DefaultFilteringBehavior.
   * @const
   */
  var kDefaultFilteringBehaviors = [
    'allow',
    'warn',
    'block'
  ];

  ManagedUserSettings.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    // The current authentication state of the manager of the managed account.
    authenticationState_: ManagedUserAuthentication.UNAUTHENTICATED,

    get isAuthenticated() {
      return !cr.isChromeOS &&
             (this.authenticationState_ ==
              ManagedUserAuthentication.AUTHENTICATED);
    },

    /** @override */
    initializePage: function() {
      var self = this;

      kDefaultFilteringBehaviors.forEach(function(value, i) {
        $('contentpacks-' + value).addEventListener('change', function() {
          self.handleSettingChanged_('ContentPackDefaultFilteringBehavior', i);
        });
      });

      $('safe-search-checkbox').addEventListener('change', function(event) {
        self.handleSettingChanged_('ForceSafeSearch', event.srcElement.checked);
      });

      $('manage-exceptions-button').onclick = function(event) {
        OptionsPage.navigateToPage('manualExceptions');
      };

      $('get-content-packs-button').onclick = function(event) {
        window.open(loadTimeData.getString('getContentPacksURL'));
      };

      if (!cr.isChromeOS) {
        $('set-passphrase').onclick = function() {
          OptionsPage.navigateToPage('setPassphrase');
        };

        var self = this;
        $('lock-settings').onclick = function() {
          chrome.send('setElevated', [false]);

          // The managed user is currently authenticated, so don't wait for a
          // callback to set the new authentication state since a reset to not
          // elevated is done without showing the passphrase dialog.
          self.authenticationState_ = ManagedUserAuthentication.UNAUTHENTICATED;
          self.updateControls_();
        };

        $('unlock-settings').onclick = function() {
        if (self.authenticationState_ == ManagedUserAuthentication.CHECKING)
            return;

        self.authenticationState_ = ManagedUserAuthentication.CHECKING;
          chrome.send('setElevated', [true]);
        };
      }

      $('managed-user-settings-done').onclick = function() {
        OptionsPage.closeOverlay();
      };
    },

    /**
     * Update the page according to the current authentication state.
     * @override
     */
    didShowPage: function() {
      this.updateControls_();
      chrome.send('settingsPageOpened');
    },

    /**
     * Reset the authentication to UNAUTHENTICATED when the managed user
     * settings dialog is closed.
     * @override
     */
    didClosePage: function() {
      // Reset the authentication of the custodian.
      this.authenticationState_ = ManagedUserAuthentication.UNAUTHENTICATED;
      chrome.send('setElevated', [false]);
      chrome.send('confirmManagedUserSettings');
    },

    /**
     * Called from the browser to update the UI with the managed user settings.
     * @param {object} settings The current managed user settings.
     */
    loadSettings_: function(settings) {
      var defaultFilteringBehavior =
          kDefaultFilteringBehaviors[
              settings.ContentPackDefaultFilteringBehavior];
      var selector = '[name="contentpacks"][value="' +
                     defaultFilteringBehavior + '"]';
      this.pageDiv.querySelector(selector).checked = true;
      $('safe-search-checkbox').checked = settings.ForceSafeSearch;
    },

    /**
     * Update a managed user setting in the browser.
     * @param {string} key The name of the setting to update.
     * @param {any} value The new value of the setting.
     */
    handleSettingChanged_: function(key, value) {
      chrome.send('setManagedUserSetting', [key, value]);
    },

    /**
     * Enables or disables all controls based on the authentication state of
     * the managed user.
     */
    updateControls_: function() {
      var locked = !this.isAuthenticated;

      Array.prototype.forEach.call(
          this.pageDiv.querySelectorAll('.disabled-when-locked'),
          function(el) {
        el.disabled = locked;
      });

      $('managed-user-settings-page').classList.toggle('locked', locked);
    },

    // Called when the passphrase dialog is closed. |success| is true iff the
    // authentication was successful.
    setAuthenticated_: function(success) {
      if (success) {
        this.authenticationState_ = ManagedUserAuthentication.AUTHENTICATED;
        this.updateControls_();
      } else {
        this.authenticationState_ = ManagedUserAuthentication.UNAUTHENTICATED;
      }
    },
  };

  ManagedUserSettings.loadSettings = function(settings) {
    ManagedUserSettings.getInstance().loadSettings_(settings);
  };

  // Sets the authentication state of the managed user. |success| is true if
  // the authentication was successful.
  ManagedUserSettings.setAuthenticated = function(success) {
    ManagedUserSettings.getInstance().setAuthenticated_(success);
  };

  var ManagedUserSettingsForTesting = {
    getSetPassphraseButton: function() {
      return $('set-passphrase');
    },
    getUnlockButton: function() {
      return $('unlock-settings');
    }
  };

  /**
   * Initializes an exceptions list.
   * @param {Array} list An array of pairs, where the first element of each pair
   *     is the filter string, and the second is the setting (allow/block).
   */
  ManagedUserSettings.setManualExceptions = function(list) {
    $('manual-exceptions').setManualExceptions(list);
  };

  /**
   * The browser's response to a request to check the validity of a given URL
   * pattern.
   * @param {string} mode The browser mode.
   * @param {string} pattern The pattern.
   * @param {bool} valid Whether said pattern is valid in the context of
   *     a content exception setting.
   */
  ManagedUserSettings.patternValidityCheckComplete =
      function(pattern, valid) {
    $('manual-exceptions').patternValidityCheckComplete(pattern, valid);
  };

  // Export
  return {
    ManagedUserSettings: ManagedUserSettings,
    ManagedUserSettingsForTesting: ManagedUserSettingsForTesting,
  };
});

}
