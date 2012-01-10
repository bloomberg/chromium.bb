// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;

  /**
   * Encapsulated handling of personal options page.
   * @constructor
   */
  function PersonalOptions() {
    OptionsPage.call(this, 'personal',
                     templateData.personalPageTabTitle,
                     'personal-page');
    if (cr.isChromeOS) {
      // Username (canonical email) of the currently logged in user or
      // |kGuestUser| if a guest session is active.
      this.username_ = localStrings.getString('username');
    }
  }

  cr.addSingletonGetter(PersonalOptions);

  PersonalOptions.prototype = {
    // Inherit PersonalOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    // Initialize PersonalOptions page.
    initializePage: function() {
      // Call base class implementation to start preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;

      // Profiles.
      var profilesList = $('profiles-list');
      options.personal_options.ProfileList.decorate(profilesList);
      profilesList.autoExpands = true;

      profilesList.onchange = self.setProfileViewButtonsStatus_;
      $('profiles-create').onclick = function(event) {
        chrome.send('createProfile');
      };
      $('profiles-manage').onclick = function(event) {
        var selectedProfile = self.getSelectedProfileItem_();
        if (selectedProfile)
          ManageProfileOverlay.showManageDialog(selectedProfile);
      };
      $('profiles-delete').onclick = function(event) {
        var selectedProfile = self.getSelectedProfileItem_();
        if (selectedProfile)
          ManageProfileOverlay.showDeleteDialog(selectedProfile);
      };

      // Appearance.
      $('themes-reset').onclick = function(event) {
        chrome.send('themesReset');
      };

      if (!cr.isChromeOS) {
        $('import-data').onclick = function(event) {
          // Make sure that any previous import success message is hidden, and
          // we're showing the UI to import further data.
          $('import-data-configure').hidden = false;
          $('import-data-success').hidden = true;
          OptionsPage.navigateToPage('importData');
          chrome.send('coreOptionsUserMetricsAction', ['Import_ShowDlg']);
        };

        if ($('themes-GTK-button')) {
          $('themes-GTK-button').onclick = function(event) {
            chrome.send('themesSetGTK');
          };
        }
      } else {
        $('change-picture-button').onclick = function(event) {
          OptionsPage.navigateToPage('changePicture');
        };
        this.updateAccountPicture_();

        if (cr.commandLine && cr.commandLine.options['--bwsi']) {
          // Disable the screen lock checkbox and change-picture-button in
          // guest mode.
          $('enable-screen-lock').disabled = true;
          $('change-picture-button').disabled = true;
        }
      }
    },

    /**
     * Get the selected profile item from the profile list. This also works
     * correctly if the list is not displayed.
     * @return {Object} the profile item object, or null if nothing is selected.
     * @private
     */
    getSelectedProfileItem_: function() {
      var profilesList = $('profiles-list');
      if (profilesList.hidden) {
        if (profilesList.dataModel.length > 0)
          return profilesList.dataModel.item(0);
      } else {
        return profilesList.selectedItem;
      }
      return null;
    },

    /**
     * Helper function to set the status of profile view buttons to disabled or
     * enabled, depending on the number of profiles and selection status of the
     * profiles list.
     */
    setProfileViewButtonsStatus_: function() {
      var profilesList = $('profiles-list');
      var selectedProfile = profilesList.selectedItem;
      var hasSelection = selectedProfile != null;
      var hasSingleProfile = profilesList.dataModel.length == 1;
      $('profiles-manage').disabled = !hasSelection ||
          !selectedProfile.isCurrentProfile;
      $('profiles-delete').disabled = !hasSelection && !hasSingleProfile;
    },

    /**
     * Display the correct dialog layout, depending on how many profiles are
     * available.
     * @param {number} numProfiles The number of profiles to display.
     */
    setProfileViewSingle_: function(numProfiles) {
      var hasSingleProfile = numProfiles == 1;
      $('profiles-list').hidden = hasSingleProfile;
      $('profiles-single-message').hidden = !hasSingleProfile;
      $('profiles-manage').hidden = hasSingleProfile;
      $('profiles-delete').textContent = hasSingleProfile ?
          templateData.profilesDeleteSingle :
          templateData.profilesDelete;
    },

    /**
     * Adds all |profiles| to the list.
     * @param {Array.<Object>} An array of profile info objects.
     *     each object is of the form:
     *       profileInfo = {
     *         name: "Profile Name",
     *         iconURL: "chrome://path/to/icon/image",
     *         filePath: "/path/to/profile/data/on/disk",
     *         isCurrentProfile: false
     *       };
     */
    setProfilesInfo_: function(profiles) {
      this.setProfileViewSingle_(profiles.length);
      // add it to the list, even if the list is hidden so we can access it
      // later.
      $('profiles-list').dataModel = new ArrayDataModel(profiles);
      this.setProfileViewButtonsStatus_();
    },

    setGtkThemeButtonEnabled_: function(enabled) {
      if (!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
        $('themes-GTK-button').disabled = !enabled;
      }
    },

    setThemesResetButtonEnabled_: function(enabled) {
      $('themes-reset').disabled = !enabled;
    },

    /**
     * (Re)loads IMG element with current user account picture.
     */
    updateAccountPicture_: function() {
      $('account-picture').src =
          'chrome://userimage/' + this.username_ +
          '?id=' + (new Date()).getTime();
    },
  };

  if (cr.isChromeOS) {
    /**
     * Returns username (canonical email) of the user logged in (ChromeOS only).
     * @return {string} user email.
     */
    PersonalOptions.getLoggedInUsername = function() {
      return PersonalOptions.getInstance().username_;
    };
  }

  // Forward public APIs to private implementations.
  [
    'setGtkThemeButtonEnabled',
    'setProfilesInfo',
    'setProfilesSectionVisible',
    'setThemesResetButtonEnabled',
    'updateAccountPicture',
  ].forEach(function(name) {
    PersonalOptions[name] = function(value) {
      return PersonalOptions.getInstance()[name + '_'](value);
    };
  });

  // Export
  return {
    PersonalOptions: PersonalOptions
  };

});
