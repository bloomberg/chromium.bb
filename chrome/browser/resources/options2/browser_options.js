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

    showHomeButton_: false,
    homePageIsNtp_: false,

    /**
     * An autocomplete list that can be attached to the home page URL text field
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
      var self = this;

      window.addEventListener('message', this.handleWindowMessage_.bind(this));

      $('advanced-settings-expander').onclick =
          this.toggleAdvancedSettings_.bind(this);

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
        options.network.NetworkList.decorate($('network-list'));
      }

      // On Startup section.
      var startupSetPagesLink = $('startup-set-pages');
      const showPagesValue = Number($('startup-show-pages').value);

      Preferences.getInstance().addEventListener('session.restore_on_startup',
                                                 function(event) {
        startupSetPagesLink.disabled = event.value['disabled'] &&
                                       event.value['value'] != showPagesValue;
      });

      startupSetPagesLink.onclick = function() {
        OptionsPage.navigateToPage('startup');
      };

      this.sessionRestoreEnabled_ = templateData.enable_restore_session_state;
      if (this.sessionRestoreEnabled_) {
        $('old-startup-last-text').hidden = true;
        $('new-startup-last-text').hidden = false;
      }

      // Appearance section.
      $('home-page-select').addEventListener(
          'change', this.onHomePageSelectChange_.bind(this));

      ['browser.show_home_button',
          'homepage',
          'homepage_is_newtabpage'].forEach(function(pref) {
        Preferences.getInstance().addEventListener(
            pref,
            self.onHomePagePrefChanged_.bind(self));
      });

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
          'session.restore_on_startup',
          this.onSessionRestoreSelectedChanged_.bind(this));
      Preferences.getInstance().addEventListener(
          'restore_session_state.dialog_shown',
          this.onSessionRestoreDialogShownChanged_.bind(this));

      // Text fields may change widths when the window changes size, so make
      // sure the suggestion list stays in sync.
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

        this.updateAccountPicture_();

        if (cr.commandLine && cr.commandLine.options['--bwsi']) {
          // Disable the screen lock checkbox in guest mode.
          $('enable-screen-lock').disabled = true;

          // Hide the startup section in Guest mode.
          $('startup-section').hidden = true;
        } else {
          $('account-picture-wrapper').onclick = function(event) {
            OptionsPage.navigateToPage('changePicture');
          };
        }

        $('manage-accounts-button').onclick = function(event) {
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

        $('auto-launch').onclick = this.handleAutoLaunchChanged_;
      }

      // Date and time section (CrOS only).
      if (cr.isChromeOS && AccountsOptions.loggedInAsGuest()) {
        // Disable time-related settings if we're not logged in as a real user.
        $('timezone-select').disabled = true;
        $('use-24hour-clock').disabled = true;
      }

      // Privacy section.
      $('privacyContentSettingsButton').onclick = function(event) {
        OptionsPage.navigateToPage('content');
        OptionsPage.showTab($('cookies-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ContentSettings']);
      };
      $('privacyClearDataButton').onclick = function(event) {
        OptionsPage.navigateToPage('clearBrowserData');
        chrome.send('coreOptionsUserMetricsAction', ['Options_ClearData']);
      };
      // 'metricsReportingEnabled' element is only present on Chrome branded
      // builds.
      if ($('metricsReportingEnabled')) {
        $('metricsReportingEnabled').onclick = function(event) {
          chrome.send('metricsReportingCheckboxAction',
              [String(event.target.checked)]);
        };
      }

      // Bluetooth (CrOS only).
      if (cr.isChromeOS) {
        options.system.bluetooth.BluetoothDeviceList.decorate(
            $('bluetooth-paired-devices-list'));

        $('bluetooth-add-device').onclick =
            this.handleAddBluetoothDevice_.bind(this);

        $('enable-bluetooth').onchange = function(event) {
          var state = $('enable-bluetooth').checked;
          chrome.send('bluetoothEnableChange', [Boolean(state)]);
        };

        $('bluetooth-reconnect-device').onclick = function(event) {
          var device = $('bluetooth-paired-devices-list').selectedItem;
          var address = device.address;
          chrome.send('updateBluetoothDevice', [address, 'connect']);
          OptionsPage.closeOverlay();
        };

        $('bluetooth-reconnect-device').onmousedown = function(event) {
          // Prevent 'blur' event, which would reset the list selection,
          // thereby disabling the apply button.
          event.preventDefault();
        };

        $('bluetooth-paired-devices-list').addEventListener('change',
            function() {
          var item = $('bluetooth-paired-devices-list').selectedItem;
          var disabled = !item || !item.paired || item.connected;
          $('bluetooth-reconnect-device').disabled = disabled;
        });
      }

      // Passwords and Forms section.
      $('autofill-settings').onclick = function(event) {
        OptionsPage.navigateToPage('autofill');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutofillSettings']);
      };
      $('manage-passwords').onclick = function(event) {
        OptionsPage.navigateToPage('passwords');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordManager']);
      };
      if (this.guestModeActive_()) {
        // Disable and turn off Autofill in guest mode.
        var autofillEnabled = $('autofill-enabled');
        autofillEnabled.disabled = true;
        autofillEnabled.checked = false;
        cr.dispatchSimpleEvent(autofillEnabled, 'change');
        $('autofill-settings').disabled = true;

        // Disable and turn off Password Manager in guest mode.
        var passwordManagerEnabled = $('password-manager-enabled');
        passwordManagerEnabled.disabled = true;
        passwordManagerEnabled.checked = false;
        cr.dispatchSimpleEvent(passwordManagerEnabled, 'change');
        $('manage-passwords').disabled = true;

        // Hide the entire section on ChromeOS
        if (cr.isChromeOS)
          $('passwords-and-autofill-section').hidden = true;
      }
      $('mac-passwords-warning').hidden =
          !(localStrings.getString('macPasswordsWarning'));

      // Network section.
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').onclick = function(event) {
          chrome.send('showNetworkProxySettings');
        };
      }

      // Web Content section.
      $('fontSettingsCustomizeFontsButton').onclick = function(event) {
        OptionsPage.navigateToPage('fonts');
        chrome.send('coreOptionsUserMetricsAction', ['Options_FontSettings']);
      };
      $('defaultFontSize').onchange = function(event) {
        var value = event.target.options[event.target.selectedIndex].value;
        Preferences.setIntegerPref(
             'webkit.webprefs.global.default_fixed_font_size',
             value - OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD, '');
        chrome.send('defaultFontSizeAction', [String(value)]);
      };
      $('defaultZoomFactor').onchange = function(event) {
        chrome.send('defaultZoomFactorAction',
            [String(event.target.options[event.target.selectedIndex].value)]);
      };

      // Languages section.
      $('language-button').onclick = function(event) {
        OptionsPage.navigateToPage('languages');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_LanuageAndSpellCheckSettings']);
      };

      // Downloads section.
      if (!cr.isChromeOS) {
        $('downloadLocationChangeButton').onclick = function(event) {
          chrome.send('selectDownloadLocation');
        };
        // This text field is always disabled. Setting ".disabled = true" isn't
        // enough, since a policy can disable it but shouldn't re-enable when
        // it is removed.
        $('downloadLocationPath').setDisabled('readonly', true);
        $('autoOpenFileTypesResetToDefault').onclick = function(event) {
          chrome.send('autoOpenFileTypesAction');
        };
      }

      // HTTPS/SSL section.
      if (cr.isWindows || cr.isMac) {
        $('certificatesManageButton').onclick = function(event) {
          chrome.send('showManageSSLCertificates');
        };
      } else {
        $('certificatesManageButton').onclick = function(event) {
          OptionsPage.navigateToPage('certificates');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ManageSSLCertificates']);
        };
      }
      $('sslCheckRevocation').onclick = function(event) {
        chrome.send('checkRevocationCheckboxAction',
            [String($('sslCheckRevocation').checked)]);
      };

      // Cloud Print section.
      // 'cloudPrintProxyEnabled' is true for Chrome branded builds on
      // certain platforms, or could be enabled by a lab.
      if (!cr.isChromeOS) {
        $('cloudPrintConnectorSetupButton').onclick = function(event) {
          if ($('cloudPrintManageButton').style.display == 'none') {
            // Disable the button, set it's text to the intermediate state.
            $('cloudPrintConnectorSetupButton').textContent =
              localStrings.getString('cloudPrintConnectorEnablingButton');
            $('cloudPrintConnectorSetupButton').disabled = true;
            chrome.send('showCloudPrintSetupDialog');
          } else {
            chrome.send('disableCloudPrintConnector');
          }
        };
      }
      $('cloudPrintManageButton').onclick = function(event) {
        chrome.send('showCloudPrintManagePage');
      };

      // Accessibility section (CrOS only).
      if (cr.isChromeOS) {
        $('accessibility-spoken-feedback-check').onchange = function(event) {
          chrome.send('spokenFeedbackChange',
          [$('accessibility-spoken-feedback-check').checked]);
        };
      }

      // Background mode section.
      if ($('backgroundModeCheckbox')) {
        $('backgroundModeCheckbox').onclick = function(event) {
          chrome.send('backgroundModeAction',
              [String($('backgroundModeCheckbox').checked)]);
        };
      }
    },

    /**
     * @inheritDoc
     */
    didShowPage: function() {
      $('search-field').focus();
    },

    /**
     * Handler for messages sent from the main uber page.
     * @param {Event} e The 'message' event from the uber page.
     * @private
     */
    handleWindowMessage_: function(e) {
      if (e.data.method == 'frameSelected')
        $('search-field').focus();
    },

    /**
     * Toggle the visibility state of the Advanced section.
     * @private
     */
    toggleAdvancedSettings_: function() {
      if ($('advanced-settings').style.height == '')
        this.showAdvancedSettings_();
      else
        this.hideAdvancedSettings_();
    },

    /**
     * Show advanced settings.
     * @private
     */
    showAdvancedSettings_: function() {
      $('advanced-settings').style.height =
          $('advanced-settings-container').offsetHeight + 20 + 'px';
      $('advanced-settings-expander').textContent =
          localStrings.getString('hideAdvancedSettings');
    },

    /**
     * Hide advanced settings.
     * @private
     */
    hideAdvancedSettings_: function() {
      $('advanced-settings').style.height = '';
      $('advanced-settings-expander').textContent =
          localStrings.getString('showAdvancedSettings');
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
      if (!syncData.syncSystemEnabled) {
        $('sync-section').hidden = true;
        return;
      }

      $('sync-section').hidden = false;
      this.syncSetupCompleted = syncData.setupCompleted;
      $('customize-sync').hidden = !syncData.setupCompleted;

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

    /**
     * Get the start/stop sync button DOM element. Used for testing.
     * @return {DOMElement} The start/stop sync button.
     * @private
     */
    getStartStopSyncButton_: function() {
      return $('start-stop-sync');
    },

    /**
     * Returns the <option> element with the given |value|.
     * @param {string} value One of 'none', 'ntp', 'url', 'choose'.
     * @return {HTMLOptionElement} the specified <option> element.
     */
    getHomePageOption_: function(value) {
      var select = $('home-page-select');
      return select.querySelector('option[value=' + value + ']');
    },

    /**
     * Selects the <option> element with the given |value|.
     * @private
     */
    selectHomePageOption_: function(value) {
      var select = $('home-page-select');
      var option = this.getHomePageOption_(value);
      if (!option.selected)
        option.selected = true;
    },

    /**
     * Event listener for the |change| event on the homepage <select> element.
     * @private
     */
    onHomePageSelectChange_: function() {
      var option = $('home-page-select').value;
      if (option == 'choose') {
        OptionsPage.navigateToPage('homePageOverlay');
        return;
      }

      var showHomeButton = (option != 'none');
      Preferences.setBooleanPref('browser.show_home_button', showHomeButton);

      if (option == 'ntp')
        Preferences.setBooleanPref('homepage_is_newtabpage', true);
      else if (option == 'url')
        Preferences.setBooleanPref('homepage_is_newtabpage', false);
    },

    /**
     * Event listener called when any homepage-related preferences change.
     * @private
     */
    onHomePagePrefChanged_: function(event) {
      switch (event.type) {
        case 'homepage':
          this.getHomePageOption_('url').textContent = event.value['value'];
          break;
        case 'browser.show_home_button':
          this.showHomeButton_ = event.value['value'];
          break;
        case 'homepage_is_newtabpage':
          this.homePageIsNtp_ = event.value['value'];
          break;
        default:
          console.error('Unexpected pref change event:', event.type);
      }
      this.updateHomePageSelector();
    },

    /**
     * Updates the homepage <select> element to have the appropriate option
     * selected.
     */
    updateHomePageSelector: function() {
      if (this.showHomeButton_) {
        if (this.homePageIsNtp_)
          this.selectHomePageOption_('ntp');
        else
          this.selectHomePageOption_('url');
      } else {
        this.selectHomePageOption_('none');
      }
    },

    /**
     * Sets the home page selector to the 'url' option.Called when user clicks
     * OK in the "Choose another..." dialog.
     */
    homePageSelectUrl: function() {
      this.selectHomePageOption_('url');
    },

   /**
    * Shows the autoLaunch preference and initializes its checkbox value.
    * @param {bool} enabled Whether autolaunch is enabled or or not.
    * @private
    */
    updateAutoLaunchState_: function(enabled) {
      $('auto-launch-option').hidden = false;
      $('auto-launch').checked = enabled;
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
      $('instant-label').htmlFor = enabled ? 'instant-field-trial-control' :
                                             'instant-enabled-control';
    },

    onSessionRestoreSelectedChanged_: function(event) {
      this.sessionRestoreSelected_ = event.value['value'] == 1;
      this.maybeShowSessionRestoreDialog_();
    },

    onSessionRestoreDialogShownChanged_: function(event) {
      this.sessionRestoreDialogShown_ = event.value['value'];
      this.maybeShowSessionRestoreDialog_();
    },

    maybeShowSessionRestoreDialog_: function() {
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
      if (!cr.isChromeOS) {
        var label = $('default-browser-state');
        label.textContent = statusString;

        $('set-as-default-browser').hidden = !canBeDefault || isDefault;
      }
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
     * Sets or clear whether Chrome should Auto-launch on computer startup.
     * @private
     */
    handleAutoLaunchChanged_: function() {
      chrome.send('toggleAutoLaunch', [$('auto-launch').checked]);
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
      var importData = $('import-data');
      if (importData) {
        importData.disabled = $('import-data').disabled = hasSelection &&
          !selectedProfile.isCurrentProfile;
      }
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
     * @param {Array.<Object>} profiles An array of profile info objects.
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

    /**
     * Handle the 'add device' button click.
     * @private
     */
    handleAddBluetoothDevice_: function() {
      $('bluetooth-unpaired-devices-list').clear();
      chrome.send('findBluetoothDevices');
      OptionsPage.navigateToPage('bluetooth');
    },

    /**
     * Set the checked state of the metrics reporting checkbox.
     * @private
     */
    setMetricsReportingCheckboxState_: function(checked, disabled) {
      $('metricsReportingEnabled').checked = checked;
      $('metricsReportingEnabled').disabled = disabled;
      if (disabled)
        $('metricsReportingEnabledText').className = 'disable-services-span';
    },

    /**
     * @private
     */
    setMetricsReportingSettingVisibility_: function(visible) {
      if (visible)
        $('metricsReportingSetting').style.display = 'block';
      else
        $('metricsReportingSetting').style.display = 'none';
    },

    /**
     * Returns whether the browser in guest mode. Some features are disabled or
     * hidden in guest mode.
     * @return {boolean} True if guest mode is currently active.
     * @private
     */
    guestModeActive_: function() {
      return cr.commandLine && cr.commandLine.options['--bwsi'];
    },

    /**
     * Set the font size selected item.
     * @private
     */
    setFontSize_: function(font_size_value) {
      var selectCtl = $('defaultFontSize');
      for (var i = 0; i < selectCtl.options.length; i++) {
        if (selectCtl.options[i].value == font_size_value) {
          selectCtl.selectedIndex = i;
          if ($('Custom'))
            selectCtl.remove($('Custom').index);
          return;
        }
      }

      // Add/Select Custom Option in the font size label list.
      if (!$('Custom')) {
        var option = new Option(localStrings.getString('fontSizeLabelCustom'),
                                -1, false, true);
        option.setAttribute('id', 'Custom');
        selectCtl.add(option);
      }
      $('Custom').selected = true;
    },

    /**
     * Populate the page zoom selector with values received from the caller.
     * @param {Array} items An array of items to populate the selector.
     *     each object is an array with three elements as follows:
     *       0: The title of the item (string).
     *       1: The value of the item (number).
     *       2: Whether the item should be selected (boolean).
     * @private
     */
    setupPageZoomSelector_: function(items) {
      var element = $('defaultZoomFactor');

      // Remove any existing content.
      element.textContent = '';

      // Insert new child nodes into select element.
      var value, title, selected;
      for (var i = 0; i < items.length; i++) {
        title = items[i][0];
        value = items[i][1];
        selected = items[i][2];
        element.appendChild(new Option(title, value, false, selected));
      }
    },

    /**
     * Set the enabled state for the autoOpenFileTypesResetToDefault button.
     * @private
     */
    setAutoOpenFileTypesDisabledAttribute_: function(disabled) {
      if (!cr.isChromeOS) {
        $('autoOpenFileTypesResetToDefault').disabled = disabled;
        if (disabled)
          $('auto-open-file-types-label').classList.add('disabled');
        else
          $('auto-open-file-types-label').classList.remove('disabled');
      }
    },

    /**
     * Set the enabled state for the proxy settings button.
     * @private
     */
    setupProxySettingsSection_: function(disabled, label) {
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').disabled = disabled;
        $('proxiesLabel').textContent = label;
      }
    },

    /**
     * Set the checked state for the sslCheckRevocation checkbox.
     * @private
     */
    setCheckRevocationCheckboxState_: function(checked, disabled) {
      $('sslCheckRevocation').checked = checked;
      $('sslCheckRevocation').disabled = disabled;
    },

    /**
     * Set the checked state for the backgroundModeCheckbox element.
     * @private
     */
    setBackgroundModeCheckboxState_: function(checked) {
      $('backgroundModeCheckbox').checked = checked;
    },

    /**
     * Set the Cloud Print proxy UI to enabled, disabled, or processing.
     * @private
     */
    setupCloudPrintConnectorSection_: function(disabled, label, allowed) {
      if (!cr.isChromeOS) {
        $('cloudPrintConnectorLabel').textContent = label;
        if (disabled || !allowed) {
          $('cloudPrintConnectorSetupButton').textContent =
            localStrings.getString('cloudPrintConnectorDisabledButton');
          $('cloudPrintManageButton').style.display = 'none';
        } else {
          $('cloudPrintConnectorSetupButton').textContent =
            localStrings.getString('cloudPrintConnectorEnabledButton');
          $('cloudPrintManageButton').style.display = 'inline';
        }
        $('cloudPrintConnectorSetupButton').disabled = !allowed;
      }
    },

    /**
     * @private
     */
    removeCloudPrintConnectorSection_: function() {
     if (!cr.isChromeOS) {
        var connectorSectionElm = $('cloud-print-connector-section');
        if (connectorSectionElm)
          connectorSectionElm.parentNode.removeChild(connectorSectionElm);
      }
    },

    /**
     * Set the initial state of the spoken feedback checkbox.
     * @private
     */
    setSpokenFeedbackCheckboxState_: function(checked) {
      $('accessibility-spoken-feedback-check').checked = checked;
    },

    /**
     * Set the initial state of the high contrast checkbox.
     * @private
     */
    setHighContrastCheckboxState_: function(checked) {
      // TODO(zork): Update UI
    },

    /**
     * Set the initial state of the screen magnifier checkbox.
     * @private
     */
    setScreenMagnifierCheckboxState_: function(checked) {
      // TODO(zork): Update UI
    },

    /**
     * Set the initial state of the virtual keyboard checkbox.
     * @private
     */
    setVirtualKeyboardCheckboxState_: function(checked) {
      // TODO(zork): Update UI
    },

    /**
     * Activate the bluetooth settings section on the System settings page.
     * @private
     */
    showBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = false;
    },

    /**
     * Dectivates the bluetooth settings section from the System settings page.
     * @private
     */
    hideBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = true;
    },

    /**
     * Sets the state of the checkbox indicating if bluetooth is turned on. The
     * state of the "Find devices" button and the list of discovered devices may
     * also be affected by a change to the state.
     * @param {boolean} checked Flag Indicating if Bluetooth is turned on.
     * @private
     */
    setBluetoothState_: function(checked) {
      $('enable-bluetooth').checked = checked;
      $('bluetooth-paired-devices-list').parentNode.hidden = !checked;
      $('bluetooth-add-device').hidden = !checked;
      $('bluetooth-reconnect-device').hidden = !checked;
      // Flush list of previously discovered devices if bluetooth is turned off.
      if (!checked) {
        $('bluetooth-paired-devices-list').clear();
        $('bluetooth-unpaired-devices-list').clear();
      } else {
        chrome.send('getPairedBluetoothDevices');
      }
    },

    /**
     * Adds an element to the list of available bluetooth devices. If an element
     * with a matching address is found, the existing element is updated.
     * @param {{name: string,
     *          address: string,
     *          icon: string,
     *          paired: boolean,
     *          connected: boolean}} device
     *     Decription of the bluetooth device.
     * @private
     */
    addBluetoothDevice_: function(device) {
      var list = $('bluetooth-unpaired-devices-list');
      if (device.paired) {
        // Test to see if the device is currently in the unpaired list, in which
        // case it should be removed from that list.
        var index = $('bluetooth-unpaired-devices-list').find(device.address);
        if (index != undefined)
          $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
        list = $('bluetooth-paired-devices-list');
      }
      list.appendDevice(device);

      // One device can be in the process of pairing.  If found, display
      // the Bluetooth pairing overlay.
      if (device.pairing)
        BluetoothPairing.showDialog(device);
    },
  };

  //Forward public APIs to private implementations.
  [
    'addBluetoothDevice',
    'getStartStopSyncButton',
    'guestModeActive',
    'hideBluetoothSettings',
    'removeCloudPrintConnectorSection',
    'setAutoOpenFileTypesDisabledAttribute',
    'setBackgroundModeCheckboxState',
    'setBluetoothState',
    'setCheckRevocationCheckboxState',
    'setFontSize',
    'setGtkThemeButtonEnabled',
    'setHighContrastCheckboxState',
    'setInstantFieldTrialStatus',
    'setMetricsReportingCheckboxState',
    'setMetricsReportingSettingVisibility',
    'setProfilesInfo',
    'setProfilesSectionVisible',
    'setScreenMagnifierCheckboxState',
    'setSpokenFeedbackCheckboxState',
    'setThemesResetButtonEnabled',
    'setupCloudPrintConnectorSection',
    'setupPageZoomSelector',
    'setupProxySettingsSection',
    'setVirtualKeyboardCheckboxState',
    'showBluetoothSettings',
    'updateAccountPicture',
    'updateAutocompleteSuggestions',
    'updateAutoLaunchState',
    'updateDefaultBrowserState',
    'updateManagedBannerVisibility',
    'updateSearchEngines',
    'updateSyncState',
    'updateStartupPages',
  ].forEach(function(name) {
    BrowserOptions[name] = function() {
      var instance = BrowserOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

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
