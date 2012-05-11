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
     * The cached value of the instant.confirm_dialog_shown preference.
     * @type {bool}
     * @private
     */
    instantConfirmDialogShown_: false,

    /**
     * True if the default cookie settings is session only. Used for deciding
     * whether to show the session restore info dialog. The value is undefined
     * until the preference has been read.
     * @type {bool}
     * @private
     */
    sessionOnlyCookies_: undefined,

    /**
     * The cached value of the spellcheck.confirm_dialog_shown preference.
     * @type {bool}
     * @private
     */
    spellcheckConfirmDialogShown_: false,

    /**
     * True if the "clear cookies and other site data on exit" setting is
     * selected. Used for deciding whether to show the session restore info
     * dialog. The value is undefined until the preference has been read.
     * @type {bool}
     * @private
     */
    clearCookiesOnExit_: undefined,

    /**
     * Keeps track of whether |onShowHomeButtonChanged_| has been called. See
     * |onShowHomeButtonChanged_|.
     * @type {bool}
     * @private
     */
    onShowHomeButtonChangedCalled_: false,

    /**
     * @inheritDoc
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      var self = this;

      window.addEventListener('message', this.handleWindowMessage_.bind(this));

      $('advanced-settings-expander').onclick = function() {
        self.toggleSectionWithAnimation_(
            $('advanced-settings'),
            $('advanced-settings-container'));
        var focusElement = $('advanced-settings-container').querySelector(
            'button, input, list, select, a');
        if (focusElement)
          focusElement.focus();
      }
      $('advanced-settings').addEventListener('webkitTransitionEnd',
          this.updateAdvancedSettingsExpander_.bind(this));

      if (cr.isChromeOS)
        UIAccountTweaks.applyGuestModeVisibility(document);

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
      Preferences.getInstance().addEventListener('session.restore_on_startup',
         this.onRestoreOnStartupChanged_.bind(this));

      $('startup-set-pages').onclick = function() {
        OptionsPage.navigateToPage('startup');
      };

      // Session restore.
      // TODO(marja): clean up the options UI after the decision on the session
      // restore changes has stabilized. For now, only the startup option is
      // renamed to "continue where I left off", but the session related content
      // settings are not disabled or overridden (because
      // templateData.enable_restore_session_state is forced to false).
      this.sessionRestoreEnabled_ = templateData.enable_restore_session_state;
      if (this.sessionRestoreEnabled_) {
        $('startup-restore-session').onclick = function(event) {
          if (!event.currentTarget.checked)
            return;

          if (!BrowserOptions.getInstance().maybeShowSessionRestoreDialog_()) {
            // The dialog is not shown; handle the event normally.
            event.currentTarget.savePrefState();
          }
        };
      }

      // Appearance section.
      Preferences.getInstance().addEventListener('browser.show_home_button',
          this.onShowHomeButtonChanged_.bind(this));

      Preferences.getInstance().addEventListener('homepage',
          this.onHomePageChanged_.bind(this));
      Preferences.getInstance().addEventListener('homepage_is_newtabpage',
          this.onHomePageIsNtpChanged_.bind(this));

      $('change-home-page').onclick = function(event) {
        OptionsPage.navigateToPage('homePageOverlay');
      };

      if ($('set-wallpaper')) {
        $('set-wallpaper').onclick = function(event) {
          OptionsPage.navigateToPage('setWallpaper');
        };
      }

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
            OptionsPage.showPageByName('instantConfirm', false);
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
          'restore_session_state.dialog_shown',
          this.onSessionRestoreDialogShownChanged_.bind(this));
      var self = this;
      Preferences.getInstance().addEventListener(
          'profile.clear_site_data_on_exit',
          function(event) {
            if (event.value && typeof event.value['value'] != 'undefined') {
              self.clearCookiesOnExit_ = event.value['value'] == true;
            }
          });

      // Users section.
      if (typeof templateData.profilesInfo != 'undefined') {
        $('profiles-section').hidden = false;

        var profilesList = $('profiles-list');
        options.browser_options.ProfileList.decorate(profilesList);
        profilesList.autoExpands = true;

        this.setProfilesInfo_(templateData.profilesInfo);

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
      }

      if (cr.isChromeOS) {
        if (!UIAccountTweaks.loggedInAsGuest()) {
          $('account-picture-wrapper').onclick = function(event) {
            OptionsPage.navigateToPage('changePicture');
          };
        }

        // Username (canonical email) of the currently logged in user or
        // |kGuestUser| if a guest session is active.
        this.username_ = localStrings.getString('username');

        this.updateAccountPicture_();

        $('manage-accounts-button').onclick = function(event) {
          OptionsPage.navigateToPage('accounts');
          chrome.send('coreOptionsUserMetricsAction',
              ['Options_ManageAccounts']);
        };
      } else {
        $('import-data').onclick = function(event) {
          ImportDataOverlay.show();
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
      // 'spelling-enabled-control' element is only present on Chrome branded
      // builds.
      if ($('spelling-enabled-control')) {
        $('spelling-enabled-control').customChangeHandler = function(event) {
          if (this.checked && !self.spellcheckConfirmDialogShown_) {
            OptionsPage.showPageByName('spellingConfirm', false);
            return true;
          }
          return false;
        };
        Preferences.getInstance().addEventListener(
            'spellcheck.confirm_dialog_shown',
            this.onSpellcheckConfirmDialogShownChanged_.bind(this));
      }
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
          var disabled = !item || item.connected;
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
      if (cr.isChromeOS && UIAccountTweaks.loggedInAsGuest()) {
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
      }

      if (cr.isMac)
        $('mac-passwords-warning').hidden = !templateData.multiple_profiles;

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
            // Disable the button, set its text to the intermediate state.
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
        cr.defineProperty($('backgroundModeCheckbox'),
            'controlledBy',
            cr.PropertyKind.ATTR);
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
     * Event listener for the 'session.restore_on_startup' pref.
     * @param {Event} event The preference change event.
     * @private
     */
    onRestoreOnStartupChanged_: function(event) {
      /** @const */ var showPagesValue = Number($('startup-show-pages').value);
      /** @const */ var showHomePageValue = 0;

      $('startup-set-pages').disabled = event.value['disabled'] &&
                                        event.value['value'] != showPagesValue;

      if (event.value['value'] == showHomePageValue) {
        // If the user previously selected "Show the homepage", the
        // preference will already be migrated to "Open a specific page". So
        // the only way to reach this code is if the 'restore on startup'
        // preference is managed.
        assert(event.value['controlledBy']);

        // Select "open the following pages" and lock down the list of URLs
        // to reflect the intention of the policy.
        $('startup-show-pages').checked = true;
        StartupOverlay.getInstance().setControlsDisabled(true);
      } else {
        // Re-enable the controls in the startup overlay if necessary.
        StartupOverlay.getInstance().updateControlStates();
      }
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
     * Shows the given section, with animation.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @private
     */
    showSectionWithAnimation_: function(section, container) {
      this.addTransitionEndListener_(section);

      // Unhide
      section.hidden = false;

      // Delay starting the transition so that hidden change will be
      // processed.
      setTimeout(function() {
        // Reveal the section using a WebKit transition.
        section.classList.add('sliding');
        section.style.height =
            container.offsetHeight + 'px';
      }, 0);
    },

    /**
     * See showSectionWithAnimation_.
     */
    hideSectionWithAnimation_: function(section, container) {
      this.addTransitionEndListener_(section);

      // Before we start hiding the section, we need to set
      // the height to a pixel value.
      section.style.height = container.offsetHeight + 'px';

      // Delay starting the transition so that the height change will be
      // processed.
      setTimeout(function() {
        // Hide the section using a WebKit transition.
        section.classList.add('sliding');
        section.style.height = '';
      }, 0);
    },

    /**
     * See showSectionWithAnimation_.
     */
    toggleSectionWithAnimation_: function(section, container) {
      if (section.style.height == '')
        this.showSectionWithAnimation_(section, container);
      else
        this.hideSectionWithAnimation_(section, container);
    },

    /**
     * Adds a |webkitTransitionEnd| listener to the given section so that
     * it can be animated. The listener will only be added to a given section
     * once, so this can be called as multiple times.
     * @param {HTMLElement} section The section to be animated.
     * @private
     */
    addTransitionEndListener_: function(section) {
      if (section.hasTransitionEndListener_)
        return;

      section.addEventListener('webkitTransitionEnd',
          this.onTransitionEnd_.bind(this));
      section.hasTransitionEndListener_ = true;
    },

    /**
     * Called after an animation transition has ended.
     * @private
     */
    onTransitionEnd_: function(event) {
      if (event.propertyName != 'height')
        return;

      var section = event.target;

      // Disable WebKit transitions.
      section.classList.remove('sliding');

      if (section.style.height == '') {
        // Hide the content so it can't get tab focus.
        section.hidden = true;
      } else {
        // Set the section height to 'auto' to allow for size changes
        // (due to font change or dynamic content).
        section.style.height = 'auto';
      }
    },

    updateAdvancedSettingsExpander_: function() {
      var expander = $('advanced-settings-expander');
      if ($('advanced-settings').style.height == '')
        expander.textContent = localStrings.getString('showAdvancedSettings');
      else
        expander.textContent = localStrings.getString('hideAdvancedSettings');
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
          syncData.setupCompleted && cr.isChromeOS;
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

      if (syncData.hasError)
        $('sync-status').classList.add('sync-error');
      else
        $('sync-status').classList.remove('sync-error');

      $('customize-sync').disabled = syncData.hasUnrecoverableError;
      // Move #enable-auto-login-checkbox to a different location on CrOS.
      if (cr.isChromeOs) {
        $('sync-general').insertBefore($('sync-status').nextSibling,
                                       $('enable-auto-login-checkbox'));
      }
      $('enable-auto-login-checkbox').hidden = !syncData.autoLoginVisible;
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
     * Event listener for the 'show home button' preference. Shows/hides the
     * UI for changing the home page with animation, unless this is the first
     * time this function is called, in which case there is no animation.
     * @param {Event} event The preference change event.
     */
    onShowHomeButtonChanged_: function(event) {
      var section = $('change-home-page-section');
      if (this.onShowHomeButtonChangedCalled_) {
        var container = $('change-home-page-section-container');
        if (event.value['value'])
          this.showSectionWithAnimation_(section, container);
        else
          this.hideSectionWithAnimation_(section, container);
      } else {
        section.hidden = !event.value['value'];
        this.onShowHomeButtonChangedCalled_ = true;
      }
    },

    /**
     * Event listener for the 'homepage is NTP' preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageIsNtpChanged_: function(event) {
      $('home-page-url').hidden = event.value['value'];
      $('home-page-ntp').hidden = !event.value['value'];
    },

    /**
     * Event listener for changes to the homepage preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageChanged_: function(event) {
      $('home-page-url').textContent = this.stripHttp_(event.value['value']);
    },

    /**
     * Removes the 'http://' from a URL, like the omnibox does. If the string
     * doesn't start with 'http://' it is returned unchanged.
     * @param {string} url The url to be processed
     * @return {string} The url with the 'http://' removed.
     */
    stripHttp_: function(url) {
      return url.replace(/^http:\/\//, '');
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

    /**
     * Called when the value of the restore_session_state.dialog_shown
     * preference changes.
     * @param {Event} event Change event.
     * @private
     */
    onSessionRestoreDialogShownChanged_: function(event) {
      this.sessionRestoreDialogShown_ = event.value['value'];
    },

    /**
     * Called when the value of the spellcheck.confirm_dialog_shown preference
     * changes. Cache this value.
     * @param {Event} event Change event.
     * @private
     */
    onSpellcheckConfirmDialogShownChanged_: function(event) {
      this.spellcheckConfirmDialogShown_ = event.value['value'];
    },

    /**
     * Displays the session restore info dialog if options depending on sessions
     * (session only cookies or clearning data on exit) are selected, and the
     * dialog has never been shown.
     * @private
     * @return {boolean} True if the dialog is shown, false otherwise.
     */
    maybeShowSessionRestoreDialog_: function() {
      // Don't show this dialog in Guest mode.
      if (cr.isChromeOS && UIAccountTweaks.loggedInAsGuest())
        return false;
      // If some of the needed preferences haven't been read yet, the
      // corresponding member variable will be undefined and we won't display
      // the dialog yet.
      if (this.userHasSelectedSessionContentSettings_() &&
          this.sessionRestoreDialogShown_ === false) {
        OptionsPage.showPageByName('sessionRestoreOverlay', false);
        return true;
      }
      return false;
    },

    /**
     * Called when the user clicks the "ok" button in the session restore
     * dialog.
     */
    sessionRestoreDialogOk: function() {
      // Set the preference.
      $('startup-restore-session').savePrefState();
      this.sessionRestoreDialogShown_ = true;
      Preferences.setBooleanPref('restore_session_state.dialog_shown', true);
    },

    /**
     * Called when the user clicks the "cancel" button in the session restore
     * dialog.
     */
    sessionRestoreDialogCancel: function() {
      // The preference was never set to "continue where I left off". Update the
      // UI to reflect the preference.
      $('startup-newtab').resetPrefState();
      $('startup-restore-session').resetPrefState();
      $('startup-show-pages').resetPrefState();
    },

    /**
     * Returns true if the user has selected content settings which rely on
     * sessions: clearning the browsing data on exit or defaulting the cookie
     * content setting to session only.
     * @private
     */
    userHasSelectedSessionContentSettings_: function() {
      return this.clearCookiesOnExit_ || this.sessionOnlyCookies_;
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
      OptionsPage.showPageByName('bluetooth', false);
    },

    /**
     * Set the checked state of the metrics reporting checkbox.
     * @private
     */
    setMetricsReportingCheckboxState_: function(checked, disabled) {
      $('metricsReportingEnabled').checked = checked;
      $('metricsReportingEnabled').disabled = disabled;
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
     * Shows/hides the autoOpenFileTypesResetToDefault button and label, with
     * animation.
     * @param {boolean} display Whether to show the button and label or not.
     * @private
     */
    setAutoOpenFileTypesDisplayed_: function(display) {
      if (cr.isChromeOS)
        return;

      if ($('advanced-settings').hidden) {
        // If the Advanced section is hidden, don't animate the transition.
        $('auto-open-file-types-section').hidden = !display;
      } else {
        if (display) {
          this.showSectionWithAnimation_(
              $('auto-open-file-types-section'),
              $('auto-open-file-types-container'));
        } else {
          this.hideSectionWithAnimation_(
              $('auto-open-file-types-section'),
              $('auto-open-file-types-container'));
        }
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
    setBackgroundModeCheckboxState_: function(
        checked, disabled, controlled_by) {
      $('backgroundModeCheckbox').checked = checked;
      $('backgroundModeCheckbox').disabled = disabled;
      $('backgroundModeCheckbox').controlledBy = controlled_by;
      OptionsPage.updateManagedBannerVisibility();
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
     * Show/hide mouse settings slider.
     * @private
     */
    showMouseControls_: function(show) {
      $('mouse-settings').hidden = !show;
    },

    /**
     * Show/hide touchpad settings slider.
     * @private
     */
    showTouchpadControls_: function(show) {
      $('touchpad-settings').hidden = !show;
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
     *          paired: boolean,
     *          bonded: boolean,
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

    /**
     * Removes an element from the list of available devices.
     * @param {string} address Unique address of the device.
     * @private
     */
    removeBluetoothDevice_: function(address) {
      var index = $('bluetooth-unpaired-devices-list').find(address);
      if (index != undefined) {
        $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
      } else {
        index = $('bluetooth-paired-devices-list').find(address);
        if (index != undefined)
          $('bluetooth-paired-devices-list').deleteItemAtIndex(index);
      }
    },

    /**
     * Called when the default content setting value for a content type changes.
     * @param {Object} dict A mapping content setting types to the default
     * value.
     * @private
     */
    setContentFilterSettingsValue_: function(dict) {
      if ('cookies' in dict && 'value' in dict['cookies'])
        this.sessionOnlyCookies_ = dict['cookies']['value'] == 'session';
    }

  };

  //Forward public APIs to private implementations.
  [
    'addBluetoothDevice',
    'getStartStopSyncButton',
    'hideBluetoothSettings',
    'removeCloudPrintConnectorSection',
    'removeBluetoothDevice',
    'setAutoOpenFileTypesDisplayed',
    'setBackgroundModeCheckboxState',
    'setBluetoothState',
    'setCheckRevocationCheckboxState',
    'setContentFilterSettingsValue',
    'setFontSize',
    'setGtkThemeButtonEnabled',
    'setHighContrastCheckboxState',
    'setInstantFieldTrialStatus',
    'setMetricsReportingCheckboxState',
    'setMetricsReportingSettingVisibility',
    'setProfilesInfo',
    'setScreenMagnifierCheckboxState',
    'setSpokenFeedbackCheckboxState',
    'setThemesResetButtonEnabled',
    'setupCloudPrintConnectorSection',
    'setupPageZoomSelector',
    'setupProxySettingsSection',
    'setVirtualKeyboardCheckboxState',
    'showBluetoothSettings',
    'showMouseControls',
    'showTouchpadControls',
    'updateAccountPicture',
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
