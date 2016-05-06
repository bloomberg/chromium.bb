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
 */
Polymer({
  is: 'settings-sync-page',

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
      observer: 'currentRouteChanged_',
    },

    /**
     * The current sync preferences, supplied by SyncBrowserProxy.
     * @type {?settings.SyncPrefs}
     */
    syncPrefs: {
      type: Object,
    },

    /**
     * Whether the "create passphrase" inputs should be shown. These inputs
     * give the user the opportunity to use a custom passphrase instead of
     * authenticating with his Google credentials.
     * @private
     */
    creatingNewPassphrase_: {
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

    /** @private {!settings.SyncBrowserProxyImpl} */
    browserProxy_: {
      type: Object,
      value: function() {
        return settings.SyncBrowserProxyImpl.getInstance();
      },
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('page-status-changed',
                          this.handlePageStatusChanged_.bind(this));
    this.addWebUIListener('sync-prefs-changed',
                          this.handleSyncPrefsChanged_.bind(this));
  },

  /** @private */
  currentRouteChanged_: function() {
    if (this.currentRoute.section == 'people' &&
        this.currentRoute.subpage.length == 1 &&
        this.currentRoute.subpage[0] == 'sync') {
      // Display loading page until the settings have been retrieved.
      this.$.pages.selected = 'loading';
      this.browserProxy_.didNavigateToSyncPage();
    } else {
      this.browserProxy_.didNavigateAwayFromSyncPage();
    }
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleSyncPrefsChanged_: function(syncPrefs) {
    this.syncPrefs = syncPrefs;

    this.askOldGooglePassphrase =
        this.syncPrefs.showPassphrase && !this.syncPrefs.usePassphrase;

    this.creatingNewPassphrase_ = false;

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

    this.onSingleSyncDataTypeChanged_();
  },

  /**
   * Handler for when any sync data type checkbox is changed.
   * @private
   */
  onSingleSyncDataTypeChanged_: function() {
    // The usePassphrase field is true if and only if the user is creating a
    // new passphrase or confirming an existing one. See the comment on the
    // syncPrefs property.
    // TODO(tommycli): Clean up the C++ handler to handle passwords separately.
    this.syncPrefs.usePassphrase = false;

    this.browserProxy_.setSyncPrefs(this.syncPrefs).then(
        this.handlePageStatusChanged_.bind(this));
  },

  /**
   * Sends the newly created custom sync passphrase to the browser.
   * @private
   */
  onSaveNewPassphraseTap_: function() {
    assert(this.creatingNewPassphrase_);

    // If a new password has been entered but it is invalid, do not send the
    // sync state to the API.
    if (!this.validateCreatedPassphrases_())
      return;

    this.syncPrefs.encryptAllData = true;
    this.syncPrefs.usePassphrase = true;
    // Custom created passphrases are never Google passphrases.
    this.syncPrefs.isGooglePassphrase = false;
    this.syncPrefs.passphrase = this.$$('#passphraseInput').value;

    this.browserProxy_.setSyncPrefs(this.syncPrefs).then(
        this.handlePageStatusChanged_.bind(this));
  },

  /**
   * Sends the user-entered existing password to re-enable sync.
   * @private
   */
  onSubmitExistingPassphraseTap_: function() {
    assert(!this.creatingNewPassphrase_);

    this.syncPrefs.usePassphrase = true;
    this.syncPrefs.isGooglePassphrase = this.askOldGooglePassphrase;

    var existingPassphraseInput = this.$$('#existingPassphraseInput');
    this.syncPrefs.passphrase = existingPassphraseInput.value;

    existingPassphraseInput.value = '';

    this.browserProxy_.setSyncPrefs(this.syncPrefs).then(
        this.handlePageStatusChanged_.bind(this));
  },

  /**
   * Called when the page status updates.
   * @param {!settings.PageStatus} pageStatus
   * @private
   */
  handlePageStatusChanged_: function(pageStatus) {
    if (pageStatus == settings.PageStatus.DONE) {
      if (this.currentRoute.section == 'people' &&
          this.currentRoute.subpage.length == 1 &&
          this.currentRoute.subpage[0] == 'sync') {
        // Event is caught by settings-animated-pages.
        this.fire('subpage-back');
      }
    } else if (pageStatus == settings.PageStatus.TIMEOUT) {
      this.$.pages.selected = 'timeout';
    } else if (pageStatus == settings.PageStatus.PASSPHRASE_FAILED) {
      this.$$('#incorrectPassphraseError').hidden = false;
    }
  },

  /**
   * Called when the encryption
   * @private
   */
  onEncryptionRadioSelectionChanged_: function(event) {
    this.creatingNewPassphrase_ =
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
