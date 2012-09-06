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
      var iconGrid = $('manage-profile-icon-grid');
      var createIconGrid = $('create-profile-icon-grid');
      options.ProfilesIconGrid.decorate(iconGrid);
      options.ProfilesIconGrid.decorate(createIconGrid);
      iconGrid.addEventListener('change', function(e) {
        self.onIconGridSelectionChanged_('manage');
      });
      createIconGrid.addEventListener('change', function(e) {
        self.onIconGridSelectionChanged_('create');
      });

      $('manage-profile-name').oninput = function(event) {
        self.onNameChanged_(event, 'manage');
      };
      $('create-profile-name').oninput = function(event) {
        self.onNameChanged_(event, 'create');
      };
      $('manage-profile-cancel').onclick =
          $('delete-profile-cancel').onclick =
              $('create-profile-cancel').onclick = function(event) {
        OptionsPage.closeOverlay();
      };
      $('manage-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        self.submitManageChanges_();
      };
      $('delete-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        chrome.send('deleteProfile', [self.profileInfo_.filePath]);
      };
      $('create-profile-ok').onclick = function(event) {
        OptionsPage.closeOverlay();
        // Get the user's chosen name and icon, or default if they do not
        // wish to customize their profile.
        var name = $('create-profile-name').value;
        var icon_url = createIconGrid.selectedItem;
        var create_checkbox = false;
        if ($('create-shortcut'))
          create_checkbox = $('create-shortcut').checked;
        chrome.send('createProfile', [name, icon_url, create_checkbox]);
      };
    },

    /** @inheritDoc */
    didShowPage: function() {
      chrome.send('requestDefaultProfileIcons');

      if ($('create-shortcut'))
        $('create-shortcut').checked = true;
      if ($('manage-shortcut'))
        $('manage-shortcut').checked = false;

      // Just ignore the manage profile dialog on Chrome OS, they use /accounts.
      if (!cr.isChromeOS && window.location.pathname == '/manageProfile')
        ManageProfileOverlay.getInstance().prepareForManageDialog_();

      $('manage-profile-name').focus();
      $('create-profile-name').focus();
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
     * @param {String} mode A label that specifies the type of dialog
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
     * @param {String} mode A label that specifies the type of dialog
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
     * @param {String} mode A label that specifies the type of dialog
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
     * @param {String} mode A label that specifies the type of dialog
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
     * Called when the user clicks "OK". Saves the newly changed profile info.
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
     * Called when the selected icon in the icon grid changes.
     * @param {String} mode A label that specifies the type of dialog
     *     box which is currently being viewed (i.e. 'create' or
     *     'manage').
     * @private
     */
    onIconGridSelectionChanged_: function(mode) {
      var iconURL = $(mode + '-profile-icon-grid').selectedItem;
      if (!iconURL || iconURL == this.iconGridSelectedURL_)
        return;
      this.iconGridSelectedURL_ = iconURL;
      chrome.send('profileIconSelectionChanged',
                  [this.profileInfo_.filePath, iconURL]);
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
     * @param {Object} profileInfo The profile object of the profile to
     *     create. Upon creation, this object only needs a name and an avatar.
     * @private
     */
    showCreateDialog_: function(profileInfo) {
      ManageProfileOverlay.setProfileInfo(profileInfo, 'create');
      $('manage-profile-overlay-create').hidden = false;
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = true;
      $('create-profile-instructions').textContent =
         loadTimeData.getStringF('createProfileInstructions');
      ManageProfileOverlay.getInstance().hideErrorBubble_('create');

      OptionsPage.showPageByName('manageProfile', false);
    },

  };

  // Forward public APIs to private implementations.
  [
    'receiveDefaultProfileIcons',
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

  // Export
  return {
    ManageProfileOverlay: ManageProfileOverlay
  };
});
