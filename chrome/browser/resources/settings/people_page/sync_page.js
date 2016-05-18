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
    /** @private */
    pages: {
      type: Object,
      value: settings.PageStatus,
      readOnly: true,
    },

    /**
     * The curerntly displayed page.
     * @private {!settings.PageStatus}
     */
    selectedPage_: {
      type: String,
      value: settings.PageStatus.SPINNER,
    },

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

    /**
     * The unload callback is needed because the sign-in flow needs to know
     * if the user has closed the tab with the sync settings. This property is
     * non-null if the user is currently navigated on the sync settings route.
     * @private {Function}
     */
    unloadCallback_: {
      type: Object,
      value: null,
    },
  },

  /** @override */
  attached: function() {
    this.addWebUIListener('page-status-changed',
                          this.handlePageStatusChanged_.bind(this));
    this.addWebUIListener('sync-prefs-changed',
                          this.handleSyncPrefsChanged_.bind(this));

    if (this.isCurrentRouteOnSyncPage_())
      this.onNavigateToPage_();
  },

  /** @override */
  detached: function() {
    if (this.isCurrentRouteOnSyncPage_())
      this.onNavigateAwayFromPage_();
  },

  /**
   * @private
   * @return {boolean} Whether the current route shows the sync page.
   */
  isCurrentRouteOnSyncPage_: function() {
    return this.currentRoute &&
        this.currentRoute.section == 'people' &&
        this.currentRoute.subpage.length == 1 &&
        this.currentRoute.subpage[0] == 'sync';
  },

  /** @private */
  currentRouteChanged_: function() {
    if (!this.isAttached)
      return;

    if (this.isCurrentRouteOnSyncPage_())
      this.onNavigateToPage_();
    else
      this.onNavigateAwayFromPage_();
  },

  /** @private */
  onNavigateToPage_: function() {
    // The element is not ready for C++ interaction until it is attached.
    assert(this.isAttached);
    assert(this.isCurrentRouteOnSyncPage_());

    if (this.unloadCallback_)
      return;

    // Display loading page until the settings have been retrieved.
    this.selectedPage_ = settings.PageStatus.SPINNER;

    this.browserProxy_.didNavigateToSyncPage();

    this.unloadCallback_ = this.onNavigateAwayFromPage_.bind(this);
    window.addEventListener('unload', this.unloadCallback_);
  },

  /** @private */
  onNavigateAwayFromPage_: function() {
    if (!this.unloadCallback_)
      return;

    this.browserProxy_.didNavigateAwayFromSyncPage();

    window.removeEventListener('unload', this.unloadCallback_);
    this.unloadCallback_ = null;
  },

  /**
   * Handler for when the sync preferences are updated.
   * @private
   */
  handleSyncPrefsChanged_: function(syncPrefs) {
    this.syncPrefs = syncPrefs;
    this.selectedPage_ = settings.PageStatus.CONFIGURE;

    // Hide the new passphrase box if the sync data has been encrypted.
    if (this.syncPrefs.encryptAllData)
      this.creatingNewPassphrase_ = false;
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
    switch (pageStatus) {
      case settings.PageStatus.SPINNER:
      case settings.PageStatus.TIMEOUT:
      case settings.PageStatus.CONFIGURE:
        this.selectedPage_ = pageStatus;
        return;
      case settings.PageStatus.DONE:
        if (this.isCurrentRouteOnSyncPage_()) {
          // Event is caught by settings-animated-pages.
          this.fire('subpage-back');
        }
        return;
      case settings.PageStatus.PASSPHRASE_FAILED:
        if (this.selectedPage_ == this.pages.CONFIGURE &&
            this.syncPrefs && this.syncPrefs.passphraseRequired) {
          this.$$('#existingPassphraseInput').invalid = true;
        }
        return;
    }

    assertNotReached();
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
    return this.syncPrefs.encryptAllData || this.creatingNewPassphrase_ ?
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
    var passphraseInput = this.$$('#passphraseInput');
    var passphraseConfirmationInput = this.$$('#passphraseConfirmationInput');

    var passphrase = passphraseInput.value;
    var confirmation = passphraseConfirmationInput.value;

    var emptyPassphrase = !passphrase;
    var mismatchedPassphrase = passphrase != confirmation;

    passphraseInput.invalid = emptyPassphrase;
    passphraseConfirmationInput.invalid =
        !emptyPassphrase && mismatchedPassphrase;

    return !emptyPassphrase && !mismatchedPassphrase;
  },
});

})();
