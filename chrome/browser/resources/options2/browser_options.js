// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var RepeatingButton = cr.ui.RepeatingButton;

  //
  // BrowserOptions class
  // Encapsulated handling of browser options page.
  //
  function BrowserOptions() {
    OptionsPage.call(this, 'settings', templateData.settingsTitle,
                     'settings');
  }

  cr.addSingletonGetter(BrowserOptions);

  BrowserOptions.prototype = {
    __proto__: options.OptionsPage.prototype,

    // State variables.
    syncSetupCompleted: false,

    /**
     * An autocomplete list that can be attached to the homepage URL text field
     * during editing.
     * @type {HTMLElement}
     * @private
     */
    autocompleteList_: null,

    /**
     * The cached value of the instant.confirm_dialog_shown preference.
     * @type {bool}
     * @private
     */
    instantConfirmDialogShown_: false,

    /**
     * @inheritDoc
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      // Sync (Sign in) section.
      this.updateSyncState_(templateData.syncData);

      $('sync-action-link').onclick = function(event) {
        SyncSetupOverlay.showErrorUI();
      };
      $('start-stop-sync').onclick = function(event) {
        if (self.syncSetupCompleted)
          SyncSetupOverlay.showStopSyncingUI();
        else
          SyncSetupOverlay.showSetupUI();
      };
      $('customize-sync').onclick = function(event) {
        SyncSetupOverlay.showSetupUI();
      };

      // Internet connection section (ChromeOS only).
      if (cr.isChromeOS) {
        $('internet-options-button').onclick = function(event) {
          OptionsPage.navigateToPage('internet');
          chrome.send('coreOptionsUserMetricsAction',
              ['Options_InternetOptions']);
        };
      }

      // On Startup section.
      $('startup-set-pages').onclick = function() {
        OptionsPage.navigateToPage('startup');
      };

      // Appearance section.
      $('change-home-page').onclick = function(event) {
        OptionsPage.navigateToPage('homePageOverlay');
      };
      $('themes-gallery').onclick = function(event) {
        window.open(localStrings.getString('themesGalleryURL'));
      };
      $('themes-reset').onclick = function(event) {
        chrome.send('themesReset');
      };

      // Device section (ChromeOS only).
      if (cr.isChromeOS) {
        $('keyboard-settings-button').onclick = function(evt) {
          OptionsPage.navigateToPage('keyboard-overlay');
        };
        $('pointer-settings-button').onclick = function(evt) {
          OptionsPage.navigateToPage('pointer-overlay');
        };
        this.initBrightnessButton_('brightness-decrease-button',
            'decreaseScreenBrightness');
        this.initBrightnessButton_('brightness-increase-button',
            'increaseScreenBrightness');
      }

      // Search section.
      $('manage-default-search-engines').onclick = function(event) {
        OptionsPage.navigateToPage('searchEngines');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_ManageSearchEngines']);
      };
      $('default-search-engine').addEventListener('change',
          this.setDefaultSearchEngine_);
      $('instant-enabled-control').customChangeHandler = function(event) {
        if (this.checked) {
          if (self.instantConfirmDialogShown_)
            chrome.send('enableInstant');
          else
            OptionsPage.navigateToPage('instantConfirm');
        } else {
          chrome.send('disableInstant');
        }
        return true;
      };
      $('instant-field-trial-control').onchange = function(evt) {
        this.checked = true;
        chrome.send('disableInstant');
      };
      Preferences.getInstance().addEventListener('instant.confirm_dialog_shown',
          this.onInstantConfirmDialogShownChanged_.bind(this));
      Preferences.getInstance().addEventListener('instant.enabled',
          this.onInstantEnabledChanged_.bind(this));
      Preferences.getInstance().addEventListener(
          "session.restore_on_startup",
          this.onSessionRestoreSelectedChanged_.bind(this));
      Preferences.getInstance().addEventListener(
          "restore_session_state.dialog_shown",
          this.onSessionRestoreDialogShownChanged_.bind(this));

      // Text fields may change widths when the window changes size, so make
      // sure the suggestion list stays in sync.
      var self = this;
      window.addEventListener('resize', function() {
        self.autocompleteList_.syncWidthToInput();
      });

      var suggestionList = new cr.ui.AutocompleteList();
      suggestionList.autoExpands = true;
      suggestionList.suggestionUpdateRequestCallback =
          this.requestAutocompleteSuggestions_.bind(this);
      $('main-content').appendChild(suggestionList);
      this.autocompleteList_ = suggestionList;

      // Users section.
      var profilesList = $('profiles-list');
      options.browser_options.ProfileList.decorate(profilesList);
      profilesList.autoExpands = true;

      profilesList.addEventListener('change',
          this.setProfileViewButtonsStatus_);
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

      if (cr.isChromeOS) {
        // Username (canonical email) of the currently logged in user or
        // |kGuestUser| if a guest session is active.
        this.username_ = localStrings.getString('username');

        $('change-picture-button').onclick = function(event) {
          OptionsPage.navigateToPage('changePicture');
        };
        this.updateAccountPicture_();

        if (cr.commandLine && cr.commandLine.options['--bwsi']) {
          // Disable the screen lock checkbox and change-picture-button in
          // guest mode.
          $('enable-screen-lock').disabled = true;
          $('change-picture-button').disabled = true;

          // Hide the startup section in Guest mode.
          $('startup-section').hidden = true;
        }

        $('manage-accounts-button').onclick, function(event) {
          OptionsPage.navigateToPage('accounts');
          chrome.send('coreOptionsUserMetricsAction',
              ['Options_ManageAccounts']);
        };
      } else {
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
      }

      // Default browser section.
      if (!cr.isChromeOS) {
        $('set-as-default-browser').onclick = function(event) {
          chrome.send('becomeDefaultBrowser');
        };
      }

      // Under the hood section.
      $('advanced-settings').onclick = function(event) {
        OptionsPage.navigateToPage('advanced');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_OpenUnderTheHood']);
      };

      this.sessionRestoreEnabled_ = templateData.enable_restore_session_state;
      if (this.sessionRestoreEnabled_) {
        $('old-startup-last-text').hidden = true;
        $('new-startup-last-text').hidden = false;
      }
    },

    /**
     * Initializes a button for controlling screen brightness.
     * @param {string} id Button ID.
     * @param {string} callback Name of the callback function.
     */
    initBrightnessButton_: function(id, callback) {
      var button = $(id);
      cr.ui.decorate(button, RepeatingButton);
      button.repeatInterval = 300;
      button.addEventListener(RepeatingButton.Event.BUTTON_HELD, function(e) {
        chrome.send(callback);
      });
    },

    /**
     * Updates the sync section with the given state.
     * @param {Object} syncData A bunch of data records that describe the status
     *     of the sync system.
     * @private
     */
    updateSyncState_: function(syncData) {
      var startStopButton = $('start-stop-sync');
      startStopButton.disabled = syncData.managed ||
          syncData.setupInProgress;
      startStopButton.hidden =
          syncData.setupCompleted && cr.isChromeOs;
      startStopButton.textContent =
          syncData.setupCompleted ?
              localStrings.getString('syncButtonTextStop') :
          syncData.setupInProgress ?
              localStrings.getString('syncButtonTextInProgress') :
              localStrings.getString('syncButtonTextStart');


      // TODO(estade): can this just be textContent?
      $('sync-status-text').innerHTML = syncData.statusText;
      var statusSet = syncData.statusText.length != 0;
      $('sync-overview').hidden = statusSet;
      $('sync-status').hidden = !statusSet;

      $('sync-action-link').textContent = syncData.actionLinkText;
      $('sync-action-link').hidden = syncData.actionLinkText.length == 0;
      $('sync-action-link').disabled = syncData.managed;

      if (syncData.syncHasError)
        $('sync-status').classList.add('sync-error');
      else
        $('sync-status').classList.remove('sync-error');

      $('customize-sync').disabled = syncData.hasUnrecoverableError;
      $('enable-auto-login-checkbox').hidden = !syncData.autoLoginVisible;
    },

    /**
     * Display or hide the profiles section of the page. This is used for
     * multi-profile settings.
     * @param {boolean} visible True to show the section.
     * @private
     */
    setProfilesSectionVisible_: function(visible) {
      $('profiles-section').hidden = !visible;
    },

    hideSyncSection_: function() {
      $('sync-section').hidden = true;
    },

    /**
     * Get the start/stop sync button DOM element. Used for testing.
     * @return {DOMElement} The start/stop sync button.
     * @private
     */
    getStartStopSyncButton_: function() {
      return $('start-stop-sync');
    },

    /**
     * Sets the label for the 'Show Home page' input.
     * @param {string} label The HTML of the input label.
     * @private
     */
    updateHomePageLabel_: function(label) {
      $('home-page-label').innerHTML = label;
    },

    /**
     * Called when the value of the instant.confirm_dialog_shown preference
     * changes. Cache this value.
     * @param {Event} event Change event.
     * @private
     */
    onInstantConfirmDialogShownChanged_: function(event) {
      this.instantConfirmDialogShown_ = event.value['value'];
    },

    /**
     * Called when the value of the instant.enabled preference changes. Request
     * the state of the Instant field trial experiment.
     * @param {Event} event Change event.
     * @private
     */
    onInstantEnabledChanged_: function(event) {
      chrome.send('getInstantFieldTrialStatus');
    },

    /**
     * Called to set the Instant field trial status.
     * @param {boolean} enabled If true, the experiment is enabled.
     * @private
     */
    setInstantFieldTrialStatus_: function(enabled) {
      $('instant-enabled-control').hidden = enabled;
      $('instant-field-trial-control').hidden = !enabled;
      $('instant-label').htmlFor = enabled ? 'instant-field-trial-control'
                                           : 'instant-enabled-control';
    },

    onSessionRestoreSelectedChanged_ : function(event) {
      this.sessionRestoreSelected_ = event.value['value'] == 1;
      this.maybeShowSessionRestoreDialog_();
    },

    onSessionRestoreDialogShownChanged_ : function(event) {
      this.sessionRestoreDialogShown_ = event.value['value'];
      this.maybeShowSessionRestoreDialog_();
    },

    maybeShowSessionRestoreDialog_ : function() {
      // If either of the needed two preferences hasn't been read yet, the
      // corresponding member variable will be undefined and we won't display
      // the dialog yet.
      if (this.sessionRestoreEnabled_ && this.sessionRestoreSelected_ &&
          this.sessionRestoreDialogShown_ === false) {
        this.sessionRestoreDialogShown_ = true;
        Preferences.setBooleanPref('restore_session_state.dialog_shown', true);
        OptionsPage.navigateToPage('sessionRestoreOverlay');
      }
    },

    /**
     * Update the Default Browsers section based on the current state.
     * @param {string} statusString Description of the current default state.
     * @param {boolean} isDefault Whether or not the browser is currently
     *     default.
     * @param {boolean} canBeDefault Whether or not the browser can be default.
     * @private
     */
    updateDefaultBrowserState_: function(statusString, isDefault,
                                         canBeDefault) {
      var label = $('default-browser-state');
      label.textContent = statusString;

      $('set-as-default-browser').hidden = !canBeDefault || isDefault;
    },

    /**
     * Clears the search engine popup.
     * @private
     */
    clearSearchEngines_: function() {
      $('default-search-engine').textContent = '';
    },

    /**
     * Updates the search engine popup with the given entries.
     * @param {Array} engines List of available search engines.
     * @param {number} defaultValue The value of the current default engine.
     * @param {boolean} defaultManaged Whether the default search provider is
     *     managed. If true, the default search provider can't be changed.
     * @private
     */
    updateSearchEngines_: function(engines, defaultValue, defaultManaged) {
      this.clearSearchEngines_();
      engineSelect = $('default-search-engine');
      engineSelect.disabled = defaultManaged;
      engineCount = engines.length;
      var defaultIndex = -1;
      for (var i = 0; i < engineCount; i++) {
        var engine = engines[i];
        var option = new Option(engine['name'], engine['index']);
        if (defaultValue == option.value)
          defaultIndex = i;
        engineSelect.appendChild(option);
      }
      if (defaultIndex >= 0)
        engineSelect.selectedIndex = defaultIndex;
    },

    /**
     * Returns true if the custom startup page control block should
     * be enabled.
     * @returns {boolean} Whether the startup page controls should be
     *     enabled.
     */
    shouldEnableCustomStartupPageControls: function(pages) {
      return $('startup-show-pages').checked &&
          !this.startup_pages_pref_.disabled;
    },

    /**
     * Sets the enabled state of the custom startup page list controls
     * based on the current startup radio button selection.
     * @private
     */
    updateCustomStartupPageControlStates_: function() {
      var disable = !this.shouldEnableCustomStartupPageControls();
      var startupPagesList = $('startupPagesList');
      startupPagesList.disabled = disable;
      startupPagesList.setAttribute('tabindex', disable ? -1 : 0);
      // Explicitly set disabled state for input text elements.
      var inputs = startupPagesList.querySelectorAll("input[type='text']");
      for (var i = 0; i < inputs.length; i++)
        inputs[i].disabled = disable;
      $('startupUseCurrentButton').disabled = disable;
    },

    /**
     * Set the default search engine based on the popup selection.
     * @private
     */
    setDefaultSearchEngine_: function() {
      var engineSelect = $('default-search-engine');
      var selectedIndex = engineSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = engineSelect.options[selectedIndex];
        chrome.send('setDefaultSearchEngine', [String(selection.value)]);
      }
    },

    /**
     * Sends an asynchronous request for new autocompletion suggestions for the
     * the given query. When new suggestions are available, the C++ handler will
     * call updateAutocompleteSuggestions_.
     * @param {string} query List of autocomplete suggestions.
     * @private
     */
    requestAutocompleteSuggestions_: function(query) {
      chrome.send('requestAutocompleteSuggestions', [query]);
    },

    /**
     * Updates the autocomplete suggestion list with the given entries.
     * @param {Array} pages List of autocomplete suggestions.
     * @private
     */
    // This function is duplicated between here and startup_overlay.js. There is
    // also some autocomplete-related duplication in the C++ handler code,
    // browser_options_handler2.cc and startup_pages_handler2.cc.
    // TODO(tbreisacher): remove the duplication by refactoring
    updateAutocompleteSuggestions_: function(suggestions) {
      var list = this.autocompleteList_;
      // If the trigger for this update was a value being selected from the
      // current list, do nothing.
      if (list.targetInput && list.selectedItem &&
          list.selectedItem['url'] == list.targetInput.value)
        return;
      list.suggestions = suggestions;
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
     * @private
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
     * @private
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
     * @private
     */
    setProfilesInfo_: function(profiles) {
      this.setProfileViewSingle_(profiles.length);
      // add it to the list, even if the list is hidden so we can access it
      // later.
      $('profiles-list').dataModel = new ArrayDataModel(profiles);
      this.setProfileViewButtonsStatus_();
    },

    setGtkThemeButtonEnabled_: function(enabled) {
      if (!cr.isChromeOS && navigator.platform.match(/linux|BSD/i))
        $('themes-GTK-button').disabled = !enabled;
    },

    setThemesResetButtonEnabled_: function(enabled) {
      $('themes-reset').disabled = !enabled;
    },

    /**
     * (Re)loads IMG element with current user account picture.
     * @private
     */
    updateAccountPicture_: function() {
      var picture = $('account-picture');
      if (picture) {
        picture.src = 'chrome://userimage/' + this.username_ + '?id=' +
            Date.now();
      }
    },
  };

  //Forward public APIs to private implementations.
  [
    'getStartStopSyncButton',
    'hideSyncSection',
    'setAutoLoginVisible',
    'setCustomizeSyncButtonEnabled',
    'setGtkThemeButtonEnabled',
    'setInstantFieldTrialStatus',
    'setProfilesInfo',
    'setProfilesSectionVisible',
    'setStartStopButtonEnabled',
    'setStartStopButtonLabel',
    'setStartStopButtonVisible',
    'setSyncActionLinkEnabled',
    'setSyncActionLinkLabel',
    'setSyncSetupCompleted',
    'setSyncStatus',
    'setSyncStatusErrorVisible',
    'setThemesResetButtonEnabled',
    'updateAccountPicture',
    'updateAutocompleteSuggestions',
    'updateHomePageLabel',
    'updateSearchEngines',
    'updateSyncState',
    'updateStartupPages',
  ].forEach(function(name) {
    BrowserOptions[name] = function() {
      var instance = BrowserOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  BrowserOptions.updateDefaultBrowserState = function(statusString, isDefault,
                                                      canBeDefault) {
    if (!cr.isChromeOS) {
      BrowserOptions.getInstance().updateDefaultBrowserState_(statusString,
                                                              isDefault,
                                                              canBeDefault);
    }
  };

  if (cr.isChromeOS) {
    /**
     * Returns username (canonical email) of the user logged in (ChromeOS only).
     * @return {string} user email.
     */
    // TODO(jhawkins): Investigate the use case for this method.
    BrowserOptions.getLoggedInUsername = function() {
      return BrowserOptions.getInstance().username_;
    };
  }

  // Export
  return {
    BrowserOptions: BrowserOptions
  };
});
