// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * ManageProfileOverlay class
   * Encapsulated handling of the 'Manage profile...' overlay page.
   * @constructor
   * @class
   */
  function ManageProfileOverlay() {
    OptionsPage.call(this, 'manageProfile',
                     loadTimeData.getString('manageProfileTabTitle'),
                     'manage-profile-overlay');
  };

  cr.addSingletonGetter(ManageProfileOverlay);

  ManageProfileOverlay.prototype = {
    // Inherit from OptionsPage.
    __proto__: OptionsPage.prototype,

    // Info about the currently managed/deleted profile.
    profileInfo_: null,

    // An object containing all known profile names.
    profileNames_: {},

    // The currently selected icon in the icon grid.
    iconGridSelectedURL_: null,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      options.ProfilesIconGrid.decorate($('manage-profile-icon-grid'));
      options.ProfilesIconGrid.decorate($('create-profile-icon-grid'));
      self.registerCommonEventHandlers_('create',
                                        self.submitCreateProfile_.bind(self));
      self.registerCommonEventHandlers_('manage',
                                        self.submitManageChanges_.bind(self));

      // Override the create-profile-ok and create-* keydown handlers, to avoid
      // closing the overlay until we finish creating the profile.
      $('create-profile-ok').onclick = function(event) {
        self.submitCreateProfile_();
      };

      $('create-profile-cancel').onclick = function(event) {
        CreateProfileOverlay.cancelCreateProfile();
      };

      $('create-profile-managed-container').hidden =
          !loadTimeData.getBoolean('managedUsersEnabled');

      $('import-existing-managed-user-link').hidden =
          !loadTimeData.getBoolean('allowCreateExistingManagedUsers') ||
          !loadTimeData.getBoolean('managedUsersEnabled');

      $('manage-profile-cancel').onclick =
          $('delete-profile-cancel').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
      $('delete-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        if (BrowserOptions.getCurrentProfile().isManaged)
          return;
        chrome.send('deleteProfile', [self.profileInfo_.filePath]);
      };
      $('add-shortcut-button').onclick = function(event) {
        chrome.send('addProfileShortcut', [self.profileInfo_.filePath]);
      };
      $('remove-shortcut-button').onclick = function(event) {
        chrome.send('removeProfileShortcut', [self.profileInfo_.filePath]);
      };

      $('create-profile-managed-signed-in-learn-more-link').onclick =
          function(event) {
        OptionsPage.navigateToPage('managedUserLearnMore');
        return false;
      };

      $('create-profile-managed-not-signed-in-link').onclick = function(event) {
        // The signin process will open an overlay to configure sync, which
        // would replace this overlay. It's smoother to close this one now.
        // TODO(pamg): Move the sync-setup overlay to a higher layer so this one
        // can stay open under it, after making sure that doesn't break anything
        // else.
        OptionsPage.closeOverlay();
        SyncSetupOverlay.startSignIn();
      };

      $('create-profile-managed-sign-in-again-link').onclick = function(event) {
        OptionsPage.closeOverlay();
        SyncSetupOverlay.showSetupUI();
      };

      $('import-existing-managed-user-link').onclick = function(event) {
        OptionsPage.closeOverlay();
        OptionsPage.navigateToPage('managedUserImport');
      };
    },

    /** @override */
    didShowPage: function() {
      chrome.send('requestDefaultProfileIcons');

      // Just ignore the manage profile dialog on Chrome OS, they use /accounts.
      if (!cr.isChromeOS && window.location.pathname == '/manageProfile')
        ManageProfileOverlay.getInstance().prepareForManageDialog_();

      // When editing a profile, initially hide the "add shortcut" and
      // "remove shortcut" buttons and ask the handler which to show. It will
      // call |receiveHasProfileShortcuts|, which will show the appropriate one.
      $('remove-shortcut-button').hidden = true;
      $('add-shortcut-button').hidden = true;

      if (loadTimeData.getBoolean('profileShortcutsEnabled')) {
        var profileInfo = ManageProfileOverlay.getInstance().profileInfo_;
        chrome.send('requestHasProfileShortcuts', [profileInfo.filePath]);
      }

      $('manage-profile-name').focus();
    },

    /**
     * Registers event handlers that are common between create and manage modes.
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @param {function()} submitFunction The function that should be called
     *     when the user chooses to submit (e.g. by clicking the OK button).
     * @private
     */
    registerCommonEventHandlers_: function(mode, submitFunction) {
      var self = this;
      $(mode + '-profile-icon-grid').addEventListener('change', function(e) {
        self.onIconGridSelectionChanged_(mode);
      });
      $(mode + '-profile-name').oninput = function(event) {
        self.onNameChanged_(event, mode);
      };
      $(mode + '-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        submitFunction();
      };
    },

    /**
     * Set the profile info used in the dialog.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       iconURL: "chrome://path/to/icon/image",
     *       filePath: "/path/to/profile/data/on/disk",
     *       isCurrentProfile: false,
     *       isManaged: false
     *     };
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @private
     */
    setProfileInfo_: function(profileInfo, mode) {
      this.iconGridSelectedURL_ = profileInfo.iconURL;
      this.profileInfo_ = profileInfo;
      $(mode + '-profile-name').value = profileInfo.name;
      $(mode + '-profile-icon-grid').selectedItem = profileInfo.iconURL;
    },

    /**
     * Sets the name of the currently edited profile.
     * @private
     */
    setProfileName_: function(name) {
      if (this.profileInfo_)
        this.profileInfo_.name = name;
      $('manage-profile-name').value = name;
    },

    /**
     * Set an array of default icon URLs. These will be added to the grid that
     * the user will use to choose their profile icon.
     * @param {Array.<string>} iconURLs An array of icon URLs.
     * @private
     */
    receiveDefaultProfileIcons_: function(iconGrid, iconURLs) {
      $(iconGrid).dataModel = new ArrayDataModel(iconURLs);

      if (this.profileInfo_)
        $(iconGrid).selectedItem = this.profileInfo_.iconURL;

      var grid = $(iconGrid);
      // Recalculate the measured item size.
      grid.measured_ = null;
      grid.columns = 0;
      grid.redraw();
    },

    /**
     * Callback to set the initial values when creating a new profile.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       iconURL: "chrome://path/to/icon/image",
     *     };
     * @private
     */
    receiveNewProfileDefaults_: function(profileInfo) {
      ManageProfileOverlay.setProfileInfo(profileInfo, 'create');
      $('create-profile-name-label').hidden = false;
      $('create-profile-name').hidden = false;
      $('create-profile-name').focus();
      $('create-profile-ok').disabled = false;
    },

    /**
     * Set a dictionary of all profile names. These are used to prevent the
     * user from naming two profiles the same.
     * @param {Object} profileNames A dictionary of profile names.
     * @private
     */
    receiveProfileNames_: function(profileNames) {
      this.profileNames_ = profileNames;
    },

    /**
     * Callback to show the add/remove shortcut buttons when in edit mode,
     * called by the handler as a result of the 'requestHasProfileShortcuts_'
     * message.
     * @param {boolean} hasShortcuts Whether profile has any existing shortcuts.
     * @private
     */
    receiveHasProfileShortcuts_: function(hasShortcuts) {
      $('add-shortcut-button').hidden = hasShortcuts;
      $('remove-shortcut-button').hidden = !hasShortcuts;
    },

    /**
     * Display the error bubble, with |errorText| in the bubble.
     * @param {string} errorText The string to display as an error.
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @param {boolean} disableOKButton True if the dialog's OK button should be
     *     disabled when the error bubble is shown. It will be (re-)enabled when
     *     the error bubble is hidden.
     * @private
     */
    showErrorBubble_: function(errorText, mode, disableOKButton) {
      var nameErrorEl = $(mode + '-profile-error-bubble');
      nameErrorEl.hidden = false;
      nameErrorEl.textContent = errorText;

      if (disableOKButton)
        $(mode + '-profile-ok').disabled = true;
    },

    /**
     * Hide the error bubble.
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @private
     */
    hideErrorBubble_: function(mode) {
      $(mode + '-profile-error-bubble').hidden = true;
      $(mode + '-profile-ok').disabled = false;
    },

    /**
     * oninput callback for <input> field.
     * @param {Event} event The event object.
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @private
     */
    onNameChanged_: function(event, mode) {
      var newName = event.target.value;
      var oldName = this.profileInfo_.name;

      if (newName == oldName) {
        this.hideErrorBubble_(mode);
      } else if (this.profileNames_[newName] != undefined) {
        var errorText =
            loadTimeData.getString('manageProfilesDuplicateNameError');
        this.showErrorBubble_(errorText, mode, true);
      } else {
        this.hideErrorBubble_(mode);

        var nameIsValid = $(mode + '-profile-name').validity.valid;
        $(mode + '-profile-ok').disabled = !nameIsValid;
      }
    },

    /**
     * Called when the user clicks "OK" or hits enter. Saves the newly changed
     * profile info.
     * @private
     */
    submitManageChanges_: function() {
      var name = $('manage-profile-name').value;
      var iconURL = $('manage-profile-icon-grid').selectedItem;

      chrome.send('setProfileIconAndName',
                  [this.profileInfo_.filePath, iconURL, name]);
    },

    /**
     * Called when the user clicks "OK" or hits enter. Creates the profile
     * using the information in the dialog.
     * @private
     */
    submitCreateProfile_: function() {
      // This is visual polish: the UI to access this should be disabled for
      // managed users, and the back end will prevent user creation anyway.
      if (this.profileInfo_ && this.profileInfo_.isManaged)
        return;

      this.hideErrorBubble_('create');
      CreateProfileOverlay.updateCreateInProgress(true);

      // Get the user's chosen name and icon, or default if they do not
      // wish to customize their profile.
      var name = $('create-profile-name').value;
      var iconUrl = $('create-profile-icon-grid').selectedItem;
      var createShortcut = $('create-shortcut').checked;
      var isManaged = $('create-profile-managed').checked;
      var existingManagedUserId = '';

      // 'createProfile' is handled by the BrowserOptionsHandler.
      chrome.send('createProfile',
                  [name, iconUrl, createShortcut,
                   isManaged, existingManagedUserId]);
    },

    /**
     * Called when the selected icon in the icon grid changes.
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @private
     */
    onIconGridSelectionChanged_: function(mode) {
      var iconURL = $(mode + '-profile-icon-grid').selectedItem;
      if (!iconURL || iconURL == this.iconGridSelectedURL_)
        return;
      this.iconGridSelectedURL_ = iconURL;
      if (this.profileInfo_ && this.profileInfo_.filePath) {
        chrome.send('profileIconSelectionChanged',
                    [this.profileInfo_.filePath, iconURL]);
      }
    },

    /**
     * Updates the contents of the "Manage Profile" section of the dialog,
     * and shows that section.
     * @private
     */
    prepareForManageDialog_: function() {
      var profileInfo = BrowserOptions.getCurrentProfile();
      ManageProfileOverlay.setProfileInfo(profileInfo, 'manage');
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = false;
      $('manage-profile-overlay-delete').hidden = true;
      $('manage-profile-name').disabled = profileInfo.isManaged;
      this.hideErrorBubble_('manage');
    },

    /**
     * Display the "Manage Profile" dialog.
     * @private
     */
    showManageDialog_: function() {
      this.prepareForManageDialog_();
      OptionsPage.navigateToPage('manageProfile');
    },

    /**
     * Display the "Delete Profile" dialog.
     * @param {Object} profileInfo The profile object of the profile to delete.
     * @private
     */
    showDeleteDialog_: function(profileInfo) {
      if (BrowserOptions.getCurrentProfile().isManaged)
        return;

      ManageProfileOverlay.setProfileInfo(profileInfo, 'manage');
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = false;
      $('delete-profile-message').textContent =
          loadTimeData.getStringF('deleteProfileMessage', profileInfo.name);
      $('delete-profile-message').style.backgroundImage = 'url("' +
          profileInfo.iconURL + '")';
      $('delete-managed-profile-addendum').hidden = !profileInfo.isManaged;

      // Because this dialog isn't useful when refreshing or as part of the
      // history, don't create a history entry for it when showing.
      OptionsPage.showPageByName('manageProfile', false);
    },

    /**
     * Display the "Create Profile" dialog.
     * @private
     */
    showCreateDialog_: function() {
      OptionsPage.navigateToPage('createProfile');
    },
  };

  // Forward public APIs to private implementations.
  [
    'receiveDefaultProfileIcons',
    'receiveNewProfileDefaults',
    'receiveProfileNames',
    'receiveHasProfileShortcuts',
    'setProfileInfo',
    'setProfileName',
    'showManageDialog',
    'showDeleteDialog',
    'showCreateDialog',
  ].forEach(function(name) {
    ManageProfileOverlay[name] = function() {
      var instance = ManageProfileOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  function CreateProfileOverlay() {
    OptionsPage.call(this, 'createProfile',
                     loadTimeData.getString('createProfileTabTitle'),
                     'manage-profile-overlay');
  };

  cr.addSingletonGetter(CreateProfileOverlay);

  CreateProfileOverlay.prototype = {
    // Inherit from ManageProfileOverlay.
    __proto__: ManageProfileOverlay.prototype,

    // The signed-in email address of the current profile, or empty if they're
    // not signed in.
    signedInEmail_: '',

    /** @override */
    canShowPage: function() {
      return !BrowserOptions.getCurrentProfile().isManaged;
    },

    /**
     * Configures the overlay to the "create user" mode.
     * @override
     */
    didShowPage: function() {
      chrome.send('requestCreateProfileUpdate');
      chrome.send('requestDefaultProfileIcons');
      chrome.send('requestNewProfileDefaults');

      $('manage-profile-overlay-create').hidden = false;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = true;
      $('create-profile-instructions').textContent =
         loadTimeData.getStringF('createProfileInstructions');
      this.hideErrorBubble_();
      this.updateCreateInProgress_(false);

      var shortcutsEnabled = loadTimeData.getBoolean('profileShortcutsEnabled');
      $('create-shortcut-container').hidden = !shortcutsEnabled;
      $('create-shortcut').checked = shortcutsEnabled;

      $('create-profile-name-label').hidden = true;
      $('create-profile-name').hidden = true;
      $('create-profile-ok').disabled = true;

      $('create-profile-managed').checked = false;
      $('create-profile-managed-signed-in').disabled = true;
      $('create-profile-managed-signed-in').hidden = true;
      $('create-profile-managed-not-signed-in').hidden = true;
    },

    /** @override */
    handleCancel: function() {
      this.cancelCreateProfile_();
    },

    /** @override */
    showErrorBubble_: function(errorText) {
      ManageProfileOverlay.getInstance().showErrorBubble_(errorText,
                                                          'create',
                                                          false);
    },

    /** @override */
    hideErrorBubble_: function() {
      ManageProfileOverlay.getInstance().hideErrorBubble_('create');
    },

    /**
     * Updates the UI when a profile create step begins or ends.
     * Note that hideErrorBubble_() also enables the "OK" button, so it
     * must be called before this function if both are used.
     * @param {boolean} inProgress True if the UI should be updated to show that
     *     profile creation is now in progress.
     * @private
     */
    updateCreateInProgress_: function(inProgress) {
      this.createInProgress_ = inProgress;
      this.updateCreateManagedUserCheckbox_();

      $('create-profile-icon-grid').disabled = inProgress;
      $('create-profile-name').disabled = inProgress;
      $('create-shortcut').disabled = inProgress;
      $('create-profile-ok').disabled = inProgress;

      $('create-profile-throbber').hidden = !inProgress;
    },

    /**
     * Cancels the creation of the a profile. It is safe to call this even
     * when no profile is in the process of being created.
     * @private
     */
    cancelCreateProfile_: function() {
      OptionsPage.closeOverlay();
      chrome.send('cancelCreateProfile');
      this.hideErrorBubble_();
      this.updateCreateInProgress_(false);
    },

    /**
     * Shows an error message describing an error when creating a new profile.
     * Called by BrowserOptions via the BrowserOptionsHandler.
     * @param {string} error The error message to display.
     * @private
     */
    onError_: function(error) {
      this.updateCreateInProgress_(false);
      this.showErrorBubble_(error);
    },

    /**
     * For new supervised users, shows a confirmation page after successfully
     * creating a new profile; otherwise, the handler will open a new window.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isManaged: (true|false),
     *     };
     * @private
     */
    onSuccess_: function(profileInfo) {
      this.updateCreateInProgress_(false);
      OptionsPage.closeOverlay();
      if (profileInfo.isManaged) {
        profileInfo.custodianEmail = this.signedInEmail_;
        ManagedUserCreateConfirmOverlay.setProfileInfo(profileInfo);
        OptionsPage.showPageByName('managedUserCreateConfirm', false);
      }
    },

    /**
     * Updates the signed-in or not-signed-in UI when in create mode. Called by
     * the handler in response to the 'requestCreateProfileUpdate' message.
     * updateManagedUsersAllowed_ is expected to be called after this is, and
     * will update additional UI elements.
     * @param {string} email The email address of the currently signed-in user.
     *     An empty string indicates that the user is not signed in.
     * @param {boolean} hasError Whether the user's sign-in credentials are
     *     still valid.
     * @private
     */
    updateSignedInStatus_: function(email, hasError) {
      this.signedInEmail_ = email;
      this.hasError_ = hasError;
      var isSignedIn = email !== '';
      $('create-profile-managed-signed-in').hidden = !isSignedIn;
      $('create-profile-managed-not-signed-in').hidden = isSignedIn;

      if (isSignedIn) {
        var accountDetailsOutOfDate =
            $('create-profile-managed-account-details-out-of-date-label');
        accountDetailsOutOfDate.textContent = loadTimeData.getStringF(
            'manageProfilesManagedAccountDetailsOutOfDate', email);
        accountDetailsOutOfDate.hidden = !hasError;

        $('create-profile-managed-signed-in-label').textContent =
            loadTimeData.getStringF(
                'manageProfilesManagedSignedInLabel', email);
        $('create-profile-managed-signed-in-label').hidden = hasError;

        $('create-profile-managed-sign-in-again-link').hidden = !hasError;
        $('create-profile-managed-signed-in-learn-more-link').hidden = hasError;
      }

      this.updateImportExistingManagedUserLink_(isSignedIn && !hasError);
    },

    /**
     * Enables/disables the 'import existing managed users' link button.
     * It also updates the button text.
     * @param {boolean} enable True to enable the link button and
     *     false otherwise.
     * @private
     */
    updateImportExistingManagedUserLink_: function(enable) {
      var importManagedUserElement = $('import-existing-managed-user-link');
      importManagedUserElement.disabled = !enable;
      importManagedUserElement.textContent = enable ?
          loadTimeData.getString('importExistingManagedUserLink') :
          loadTimeData.getString('signInToImportManagedUsers');
    },

    /**
     * Sets whether creating managed users is allowed or not. Called by the
     * handler in response to the 'requestCreateProfileUpdate' message or a
     * change in the (policy-controlled) pref that prohibits creating managed
     * users, after the signed-in status has been updated.
     * @param {boolean} allowed True if creating managed users should be
     *     allowed.
     * @private
     */
    updateManagedUsersAllowed_: function(allowed) {
      this.managedUsersAllowed_ = allowed;
      this.updateCreateManagedUserCheckbox_();

      $('create-profile-managed-not-signed-in-link').hidden = !allowed;
      if (!allowed) {
        $('create-profile-managed-indicator').setAttribute('controlled-by',
                                                           'policy');
      } else {
        $('create-profile-managed-indicator').removeAttribute('controlled-by');
      }
    },

    /**
     * Updates the status of the "create managed user" checkbox. Called from
     * updateManagedUsersAllowed_() or updateCreateInProgress_().
     * updateSignedInStatus_() does not call this method directly, because it
     * will be followed by a call to updateManagedUsersAllowed_().
     * @private
     */
    updateCreateManagedUserCheckbox_: function() {
      $('create-profile-managed').disabled =
          !this.managedUsersAllowed_ || this.createInProgress_ ||
          this.signedInEmail_ == '' || this.hasError_;
    },
  };

  // Forward public APIs to private implementations.
  [
    'cancelCreateProfile',
    'onError',
    'onSuccess',
    'updateCreateInProgress',
    'updateManagedUsersAllowed',
    'updateSignedInStatus',
  ].forEach(function(name) {
    CreateProfileOverlay[name] = function() {
      var instance = CreateProfileOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ManageProfileOverlay: ManageProfileOverlay,
    CreateProfileOverlay: CreateProfileOverlay,
  };
});
