// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  const localStrings = new LocalStrings();

  /**
   * ManageProfileOverlay class
   * Encapsulated handling of the 'Manage profile...' overlay page.
   * @constructor
   * @class
   */
  function ManageProfileOverlay() {
    OptionsPage.call(this,
                     'manageProfile',
                     templateData.manageProfileOverlayTabTitle,
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

    /**
     * Initialize the page.
     */
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      var iconGrid = $('manage-profile-icon-grid');
      options.ProfilesIconGrid.decorate(iconGrid);

      $('manage-profile-name').oninput = this.onNameChanged_.bind(this);
      $('manage-profile-cancel').onclick =
          $('delete-profile-cancel').onclick = function(event) {
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
    },

    /** @inheritDoc */
    didShowPage: function() {
      chrome.send('requestDefaultProfileIcons');

      // Use the hash to specify the profile index.
      var hash = location.hash;
      if (hash) {
        $('manage-profile-overlay-manage').hidden = false;
        $('manage-profile-overlay-delete').hidden = true;
        ManageProfileOverlay.getInstance().hideErrorBubble_();

        chrome.send('requestProfileInfo', [parseInt(hash.slice(1), 10)]);
      }

      $('manage-profile-name').focus();
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
     * @private
     */
    setProfileInfo_: function(profileInfo) {
      this.profileInfo_ = profileInfo;
      $('manage-profile-name').value = profileInfo.name;
      $('manage-profile-icon-grid').selectedItem = profileInfo.iconURL;
    },

    /**
     * Set an array of default icon URLs. These will be added to the grid that
     * the user will use to choose their profile icon.
     * @param {Array.<string>} iconURLs An array of icon URLs.
     * @private
     */
    receiveDefaultProfileIcons_: function(iconURLs) {
      $('manage-profile-icon-grid').dataModel = new ArrayDataModel(iconURLs);

      // Changing the dataModel resets the selectedItem. Re-select it, if there
      // is one.
      if (this.profileInfo_)
        $('manage-profile-icon-grid').selectedItem = this.profileInfo_.iconURL;

      var grid = $('manage-profile-icon-grid');
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
     * @private
     */
    showErrorBubble_: function(errorText) {
      var nameErrorEl = $('manage-profile-error-bubble');
      nameErrorEl.hidden = false;
      nameErrorEl.textContent = localStrings.getString(errorText);

      $('manage-profile-ok').disabled = true;
    },

    /**
     * Hide the error bubble.
     * @private
     */
    hideErrorBubble_: function() {
      $('manage-profile-error-bubble').hidden = true;
      $('manage-profile-ok').disabled = false;
    },

    /**
     * oninput callback for <input> field.
     * @param event The event object
     * @private
     */
    onNameChanged_: function(event) {
      var newName = event.target.value;
      var oldName = this.profileInfo_.name;

      if (newName == oldName) {
        this.hideErrorBubble_();
      } else if (this.profileNames_[newName] != undefined) {
        this.showErrorBubble_('manageProfilesDuplicateNameError');
      } else {
        this.hideErrorBubble_();

        var nameIsValid = $('manage-profile-name').validity.valid;
        $('manage-profile-ok').disabled = !nameIsValid;
      }
    },

    /**
     * Called when the user clicks "OK". Saves the newly changed profile info.
     * @private
     */
    submitManageChanges_: function() {
      var name = $('manage-profile-name').value;
      var iconURL = $('manage-profile-icon-grid').selectedItem;
      chrome.send('setProfileNameAndIcon',
                  [this.profileInfo_.filePath, name, iconURL]);
    },

    /**
     * Display the "Manage Profile" dialog.
     * @param {Object} profileInfo The profile object of the profile to manage.
     * @private
     */
    showManageDialog_: function(profileInfo) {
      ManageProfileOverlay.setProfileInfo(profileInfo);
      $('manage-profile-overlay-manage').hidden = false;
      $('manage-profile-overlay-delete').hidden = true;
      ManageProfileOverlay.getInstance().hideErrorBubble_();

      // Intentionally don't show the URL in the location bar as we don't want
      // people trying to navigate here by hand.
      OptionsPage.showPageByName('manageProfile', false);
    },

    /**
     * Display the "Delete Profile" dialog.
     * @param {Object} profileInfo The profile object of the profile to delete.
     * @private
     */
    showDeleteDialog_: function(profileInfo) {
      ManageProfileOverlay.setProfileInfo(profileInfo);
      $('manage-profile-overlay-manage').hidden = true;
      $('manage-profile-overlay-delete').hidden = false;
      $('delete-profile-message').textContent =
          localStrings.getStringF('deleteProfileMessage', profileInfo.name);

      // Intentionally don't show the URL in the location bar as we don't want
      // people trying to navigate here by hand.
      OptionsPage.showPageByName('manageProfile', false);
    },
  };

  // Forward public APIs to private implementations.
  [
    'receiveDefaultProfileIcons',
    'receiveProfileNames',
    'setProfileInfo',
    'showManageDialog',
    'showDeleteDialog',
  ].forEach(function(name) {
    ManageProfileOverlay[name] = function(value) {
      ManageProfileOverlay.getInstance()[name + '_'](value);
    };
  });

  // Export
  return {
    ManageProfileOverlay: ManageProfileOverlay
  };
});
