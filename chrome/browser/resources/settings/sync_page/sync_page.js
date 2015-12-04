// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

/**
 * Names of the radio buttons which allow the user to choose his encryption
 * mechanism.
 * @enum {string}
 */
var RadioButtonNames = {
  ENCRYPT_WITH_GOOGLE: 'encrypt-with-google',
  ENCRYPT_WITH_PASSPHRASE: 'encrypt-with-passphrase',
};

/**
 * @fileoverview
 * 'settings-sync-page' is the settings page containing sync settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-sync-page></settings-sync-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element settings-sync-page
 */
Polymer({
  is: 'settings-sync-page',

  behaviors: [
    I18nBehavior,
  ],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      observer: 'currentRouteChanged_',
    },

    /**
     * The current sync preferences, supplied by settings.SyncPrivateApi.
     * @type {?settings.SyncPrefs}
     */
    syncPrefs: {
      type: Object,
    },

    /**
     * Whether the "create passphrase" inputs should be shown. These inputs
     * give the user the opportunity to use a custom passphrase instead of
     * authenticating with his Google credentials.
     */
    creatingNewPassphrase: {
      type: Boolean,
      value: false,
    },

    /**
     * True if subpage needs the user's old Google password. This can happen
     * when the user changes his password after encrypting his sync data.
     *
     * TODO(tommycli): FROM the C++ handler, the syncPrefs.usePassphrase field
     * is true if and only if there is a custom non-Google Sync password.
     *
     * But going TO the C++ handler, the syncPrefs.usePassphrase field is true
     * if there is either a custom or Google password. There is a separate
     * syncPrefs.isGooglePassphrase field.
     *
     * We keep an extra state variable here because we mutate the
     * syncPrefs.usePassphrase field in the OK button handler.
     * Remove this once we fix refactor the legacy SyncSetupHandler.
     */
    askOldGooglePassphrase: {
      type: Boolean,
      value: false,
    },
  },

  created: function() {
    settings.SyncPrivateApi.setSyncPrefsCallback(
        this.handleSyncPrefsFetched_.bind(this));
  },

  /** @private */
  currentRouteChanged_: function() {
    if (this.currentRoute.section == 'people' &&
        this.currentRoute.subpage.length == 1 &&
        this.currentRoute.subpage[0] == 'sync') {
      // Display loading page until the settings have been retrieved.
      this.$.pages.selected = 'loading';
      settings.SyncPrivateApi.didNavigateToSyncPage();
    } else {
      settings.SyncPrivateApi.didNavigateAwayFromSyncPage();
    }
  },

  /**
   * Handler for when the sync state is pushed from settings.SyncPrivateApi.
   * @private
   */
  handleSyncPrefsFetched_: function(syncPrefs) {
    this.syncPrefs = syncPrefs;

    this.askOldGooglePassphrase =
        this.syncPrefs.showPassphrase && !this.syncPrefs.usePassphrase;

    this.creatingNewPassphrase = false;

    this.$.pages.selected = 'main';
  },

  /**
   * Handler for when the sync all data types checkbox is changed.
   * @param {Event} event
   * @private
   */
  onSyncAllDataTypesChanged_: function(event) {
    if (event.target.checked) {
      this.set('syncPrefs.syncAllDataTypes', true);
      this.set('syncPrefs.appsSynced', true);
      this.set('syncPrefs.extensionsSynced', true);
      this.set('syncPrefs.preferencesSynced', true);
      this.set('syncPrefs.autofillSynced', true);
      this.set('syncPrefs.typedUrlsSynced', true);
      this.set('syncPrefs.themesSynced', true);
      this.set('syncPrefs.bookmarksSynced', true);
      this.set('syncPrefs.passwordsSynced', true);
      this.set('syncPrefs.tabsSynced', true);
    }
  },

  /** @private */
  onCancelTap_: function() {
    // Event is caught by settings-animated-pages.
    this.fire('subpage-back');
  },

  /**
   * Sets the sync data by sending it to the settings.SyncPrivateApi.
   * @private
   */
  onOkTap_: function() {
    if (this.creatingNewPassphrase) {
      // If a new password has been entered but it is invalid, do not send the
      // sync state to the API.
      if (!this.validateCreatedPassphrases_())
        return;

      this.syncPrefs.encryptAllData = true;
    }

    this.syncPrefs.isGooglePassphrase = this.askOldGooglePassphrase;
    this.syncPrefs.usePassphrase =
        this.creatingNewPassphrase || this.syncPrefs.showPassphrase;

    if (this.syncPrefs.usePassphrase) {
      var field = this.creatingNewPassphrase ?
          this.$$('#passphraseInput') : this.$$('#existingPassphraseInput');
      this.syncPrefs.passphrase = field.value;
      field.value = '';
    }

    settings.SyncPrivateApi.setSyncPrefs(
        this.syncPrefs, this.setPageStatusCallback_.bind(this));
  },

  /**
   * Callback invoked from calling settings.SyncPrivateApi.setSyncPrefs().
   * @param {!settings.PageStatus} callbackState
   * @private
   */
  setPageStatusCallback_: function(callbackState) {
    if (callbackState == settings.PageStatus.DONE) {
      this.onCancelTap_();
    } else if (callbackState == settings.PageStatus.TIMEOUT) {
      this.$.pages.selected = 'timeout';
    } else if (callbackState ==
               settings.PageStatus.PASSPHRASE_ERROR) {
      this.$$('#incorrectPassphraseError').hidden = false;
    }
  },

  /**
   * Called when the encryption
   * @private
   */
  onEncryptionRadioSelectionChanged_: function(event) {
    this.creatingNewPassphrase =
        event.target.selected == RadioButtonNames.ENCRYPT_WITH_PASSPHRASE;
  },

  /**
   * Computed binding returning the selected encryption radio button.
   * @private
   */
  selectedEncryptionRadio_: function() {
    return this.encryptionRadiosDisabled_() ?
        RadioButtonNames.ENCRYPT_WITH_PASSPHRASE :
        RadioButtonNames.ENCRYPT_WITH_GOOGLE;
  },

  /**
   * Computed binding returning the selected encryption radio button.
   * @private
   */
  encryptionRadiosDisabled_: function() {
    return this.syncPrefs.usePassphrase || this.syncPrefs.encryptAllData;
  },

  /**
   * Computed binding returning the encryption text body.
   * @private
   */
  encryptWithPassphraseBody_: function() {
    if (this.syncPrefs && this.syncPrefs.fullEncryptionBody)
      return this.syncPrefs.fullEncryptionBody;

    return this.i18n('encryptWithSyncPassphraseLabel');
  },

  /**
   * @param {boolean} syncAllDataTypes
   * @param {boolean} enforced
   * @return {boolean} Whether the sync checkbox should be disabled.
   */
  shouldSyncCheckboxBeDisabled_: function(syncAllDataTypes, enforced) {
    return syncAllDataTypes || enforced;
  },

  /**
   * Checks the supplied passphrases to ensure that they are not empty and that
   * they match each other. Additionally, displays error UI if they are
   * invalid.
   * @return {boolean} Whether the check was successful (i.e., that the
   *     passphrases were valid).
   * @private
   */
  validateCreatedPassphrases_: function() {
    this.$$('#emptyPassphraseError').hidden = true;
    this.$$('#mismatchedPassphraseError').hidden = true;

    var passphrase = this.$$('#passphraseInput').value;
    if (!passphrase) {
      this.$$('#emptyPassphraseError').hidden = false;
      return false;
    }

    var confirmation = this.$$('#passphraseConfirmationInput').value;
    if (passphrase != confirmation) {
      this.$$('#mismatchedPassphraseError').hidden = false;
      return false;
    }

    return true;
  },
});

})();
