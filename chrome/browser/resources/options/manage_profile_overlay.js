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

      if (loadTimeData.getBoolean('managedUsersEnabled')) {
        $('create-profile-managed-container').hidden = false;
        $('managed-user-settings-button').onclick = function(event) {
          OptionsPage.navigateToPage('managedUser');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ManagedUserPassphraseOverlay']);
        };
      }
      $('manage-profile-cancel').onclick =
          $('delete-profile-cancel').onclick =
              $('create-profile-cancel').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
      $('delete-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        chrome.send('deleteProfile', [self.profileInfo_.filePath]);
      };
    },

    /** @override */
    didShowPage: function() {
      chrome.send('requestDefaultProfileIcons');

      if ($('manage-shortcut'))
        $('manage-shortcut').checked = false;

      // Just ignore the manage profile dialog on Chrome OS, they use /accounts.
      if (!cr.isChromeOS && window.location.pathname == '/manageProfile')
        ManageProfileOverlay.getInstance().prepareForManageDialog_();

      $('manage-profile-name').focus();
    },

    /**
     * Registers event handlers that are common between create and manage modes.
     * @param {String} mode A label that specifies the type of dialog
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
      $(mode + '-profile-name').onkeydown =
          $(mode + '-profile-icon-grid').onkeydown = function(event) {
        // Submit if the OK button is enabled and we hit enter.
        if (!$(mode + '-profile-ok').disabled && event.keyCode == 13) {
          OptionsPage.closeOverlay();
          submitFunction();
        }
      };
    },

    /**
     * Set the profile info used in the dialog.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       iconURL: "chrome://path/to/icon/image",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isCurrentProfile: false,
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
      $('managed-user-settings-button').hidden =
          !loadTimeData.getBoolean('managedUsersEnabled') ||
          !profileInfo.isManaged;
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
     * Display the error bubble, with |errorText| in the bubble.
     * @param {string} errorText The localized string id to display as an error.
     * @param {string} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @private
     */
    showErrorBubble_: function(errorText, mode) {
      var nameErrorEl = $(mode + '-profile-error-bubble');
      nameErrorEl.hidden = false;
      nameErrorEl.textContent = loadTimeData.getString(errorText);

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
        this.showErrorBubble_('manageProfilesDuplicateNameError', mode);
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
      var manage_checkbox = false;
      if ($('manage-shortcut'))
        manage_checkbox = $('manage-shortcut').checked;
      chrome.send('setProfileNameAndIcon',
                  [this.profileInfo_.filePath, name, iconURL,
                  manage_checkbox]);
    },

    /**
     * Called when the user clicks "OK" or hits enter. Creates the profile
     * using the information in the dialog.
     * @private
     */
    submitCreateProfile_: function() {
      // Get the user's chosen name and icon, or default if they do not
      // wish to customize their profile.
      var name = $('create-profile-name').value;
      var icon_url = $('create-profile-icon-grid').selectedItem;
      var create_shortcut = false;
      if ($('create-shortcut'))
        create_checkbox = $('create-shortcut').checked;
      var is_managed = $('create-profile-managed').checked;
      chrome.send('createProfile',
                  [name, icon_url, create_shortcut, is_managed]);
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
      ManageProfileOverlay.setProfileInfo(profileInfo, 'manage');
      $('manage-profile-overlay-create').hidden = true;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = false;
      $('delete-profile-message').textContent =
          loadTimeData.getStringF('deleteProfileMessage', profileInfo.name);
      $('delete-profile-message').style.backgroundImage = 'url("' +
          profileInfo.iconURL + '")';

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

    /**
     * Configures the overlay to the "create user" mode.
     * @override
     */
    didShowPage: function() {
      chrome.send('requestDefaultProfileIcons');
      chrome.send('requestNewProfileDefaults');

      $('manage-profile-overlay-create').hidden = false;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = true;
      $('create-profile-instructions').textContent =
         loadTimeData.getStringF('createProfileInstructions');
      ManageProfileOverlay.getInstance().hideErrorBubble_('create');

      if ($('create-shortcut'))
        $('create-shortcut').checked = true;
      $('create-profile-name-label').hidden = true;
      $('create-profile-name').hidden = true;
      $('create-profile-ok').disabled = true;
    },
  };

  // Export
  return {
    ManageProfileOverlay: ManageProfileOverlay,
    CreateProfileOverlay: CreateProfileOverlay,
  };
});
