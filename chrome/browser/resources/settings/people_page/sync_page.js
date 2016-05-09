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
    this.browserProxy_.setSyncDatatypes(this.syncPrefs).then(
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
    this.syncPrefs.setNewPassphrase = true;
    this.syncPrefs.passphrase = this.$$('#passphraseInput').value;

    this.browserProxy_.setSyncEncryption(this.syncPrefs).then(
        this.handlePageStatusChanged_.bind(this));
  },

  /**
   * Sends the user-entered existing password to re-enable sync.
   * @private
   */
  onSubmitExistingPassphraseTap_: function() {
    assert(!this.creatingNewPassphrase_);

    this.syncPrefs.setNewPassphrase = false;

    var existingPassphraseInput = this.$$('#existingPassphraseInput');
    this.syncPrefs.passphrase = existingPassphraseInput.value;
    existingPassphraseInput.value = '';

    this.browserProxy_.setSyncEncryption(this.syncPrefs).then(
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
    return this.syncPrefs.passphraseTypeIsCustom ?
        RadioButtonNames.ENCRYPT_WITH_PASSPHRASE :
        RadioButtonNames.ENCRYPT_WITH_GOOGLE;
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
