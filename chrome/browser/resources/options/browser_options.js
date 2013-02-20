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
    OptionsPage.call(this, 'settings', loadTimeData.getString('settingsTitle'),
                     'settings');
  }

  cr.addSingletonGetter(BrowserOptions);

  BrowserOptions.prototype = {
    __proto__: options.OptionsPage.prototype,

    // State variables.
    signedIn: false,

    /**
     * Keeps track of whether |onShowHomeButtonChanged_| has been called. See
     * |onShowHomeButtonChanged_|.
     * @type {bool}
     * @private
     */
    onShowHomeButtonChangedCalled_: false,

    /**
     * Track if page initialization is complete.  All C++ UI handlers have the
     * chance to manipulate page content within their InitializePage mathods.
     * This flag is set to true after all initializers have been called.
     * @type (boolean}
     * @private
     */
    initializationComplete_: false,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      var self = this;

      // Ensure that navigation events are unblocked on uber page. A reload of
      // the settings page while an overlay is open would otherwise leave uber
      // page in a blocked state, where tab switching is not possible.
      uber.invokeMethodOnParent('stopInterceptingEvents');

      window.addEventListener('message', this.handleWindowMessage_.bind(this));

      $('advanced-settings-expander').onclick = function() {
        self.toggleSectionWithAnimation_(
            $('advanced-settings'),
            $('advanced-settings-container'));

        // If the link was focused (i.e., it was activated using the keyboard)
        // and it was used to show the section (rather than hiding it), focus
        // the first element in the container.
        if (document.activeElement === $('advanced-settings-expander') &&
                $('advanced-settings').style.height === '') {
          var focusElement = $('advanced-settings-container').querySelector(
              'button, input, list, select, a[href]');
          if (focusElement)
            focusElement.focus();
        }
      }

      $('advanced-settings').addEventListener('webkitTransitionEnd',
          this.updateAdvancedSettingsExpander_.bind(this));

      if (cr.isChromeOS)
        UIAccountTweaks.applyGuestModeVisibility(document);

      // Sync (Sign in) section.
      this.updateSyncState_(loadTimeData.getValue('syncData'));

      $('start-stop-sync').onclick = function(event) {
        if (self.signedIn)
          SyncSetupOverlay.showStopSyncingUI();
        else if (cr.isChromeOS)
          SyncSetupOverlay.showSetupUIWithoutLogin();
        else
          SyncSetupOverlay.showSetupUI();
      };
      $('customize-sync').onclick = function(event) {
        if (cr.isChromeOS)
          SyncSetupOverlay.showSetupUIWithoutLogin();
        else
          SyncSetupOverlay.showSetupUI();
      };

      // Internet connection section (ChromeOS only).
      if (cr.isChromeOS) {
        options.network.NetworkList.decorate($('network-list'));
        options.network.NetworkList.refreshNetworkData(
            loadTimeData.getValue('networkData'));
      }

      // On Startup section.
      Preferences.getInstance().addEventListener('session.restore_on_startup',
          this.onRestoreOnStartupChanged_.bind(this));
      Preferences.getInstance().addEventListener(
          'session.urls_to_restore_on_startup',
          function(event) {
            $('startup-set-pages').disabled = event.value.disabled;
          });

      $('startup-set-pages').onclick = function() {
        OptionsPage.navigateToPage('startup');
      };

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
          chrome.send('openWallpaperManager');
        };
      }

      $('themes-gallery').onclick = function(event) {
        window.open(loadTimeData.getString('themesGalleryURL'));
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
      if (loadTimeData.getValue('instant_enabled') ==
          'instant_extended.enabled') {
        // We don't want to see the confirm dialog for instant extended.
        $('instant-enabled-control').removeAttribute('dialog-pref');
        $('instant-enabled-indicator').removeAttribute('dialog-pref');
        // And we want to upload a different metric name.
        $('instant-enabled-control').setAttribute(
            'metric', 'Options_InstantExtendedEnabled');
      }

      // Users section.
      if (loadTimeData.valueExists('profilesInfo')) {
        $('profiles-section').hidden = false;

        var profilesList = $('profiles-list');
        options.browser_options.ProfileList.decorate(profilesList);
        profilesList.autoExpands = true;

        this.setProfilesInfo_(loadTimeData.getValue('profilesInfo'));

        profilesList.addEventListener('change',
            this.setProfileViewButtonsStatus_);
        $('profiles-create').onclick = function(event) {
          ManageProfileOverlay.showCreateDialog();
        };
        if (OptionsPage.isSettingsApp()) {
          $('profiles-app-list-switch').onclick = function(event) {
            var selectedProfile = self.getSelectedProfileItem_();
            chrome.send('switchAppListProfile', [selectedProfile.filePath]);
          };
        }
        $('profiles-manage').onclick = function(event) {
          ManageProfileOverlay.showManageDialog();
        };
        $('profiles-delete').onclick = function(event) {
          var selectedProfile = self.getSelectedProfileItem_();
          if (selectedProfile)
            ManageProfileOverlay.showDeleteDialog(selectedProfile);
        };
        if (loadTimeData.getBoolean('profileIsManaged')) {
          $('profiles-create').disabled = true;
          $('profiles-delete').disabled = true;
        }
      }

      if (cr.isChromeOS) {
        if (!UIAccountTweaks.loggedInAsGuest()) {
          $('account-picture-wrapper').onclick = function(event) {
            OptionsPage.navigateToPage('changePicture');
          };
        }

        // Username (canonical email) of the currently logged in user or
        // |kGuestUser| if a guest session is active.
        this.username_ = loadTimeData.getString('username');

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
      $('privacyClearDataButton').hidden = OptionsPage.isSettingsApp();
      // 'metricsReportingEnabled' element is only present on Chrome branded
      // builds.
      if ($('metricsReportingEnabled')) {
        $('metricsReportingEnabled').onclick = function(event) {
          chrome.send('metricsReportingCheckboxAction',
              [String(event.currentTarget.checked)]);
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

      if (cr.isMac) {
        $('mac-passwords-warning').hidden =
            !loadTimeData.getBoolean('multiple_profiles');
      }

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
             'webkit.webprefs.default_fixed_font_size',
             value - OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD, true);
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
      Preferences.getInstance().addEventListener('download.default_directory',
          this.onDefaultDownloadDirectoryChanged_.bind(this));
      $('downloadLocationChangeButton').onclick = function(event) {
        chrome.send('selectDownloadLocation');
      };
      if (!cr.isChromeOS) {
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

      // Cloud Print section.
      // 'cloudPrintProxyEnabled' is true for Chrome branded builds on
      // certain platforms, or could be enabled by a lab.
      if (!cr.isChromeOS) {
        $('cloudPrintConnectorSetupButton').onclick = function(event) {
          if ($('cloudPrintManageButton').style.display == 'none') {
            // Disable the button, set its text to the intermediate state.
            $('cloudPrintConnectorSetupButton').textContent =
              loadTimeData.getString('cloudPrintConnectorEnablingButton');
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

        $('accessibility-high-contrast-check').onchange = function(event) {
          chrome.send('highContrastChange',
                      [$('accessibility-high-contrast-check').checked]);
        };

        // Disable the magnifier-type dropdown list when the magnifier is
        // disabled.
        Preferences.getInstance().addEventListener(
            'settings.a11y.screen_magnifier',
            function(event) {
              $('accessibility-screen-magnifier-type-select').setDisabled(
                  'magnifier-is-disabled', !event.value.value);
            });

      }

      // Display management section (CrOS only).
      if (cr.isChromeOS) {
        $('display-options').onclick = function(event) {
          OptionsPage.navigateToPage('display');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_Display']);
        };
      }

      // Factory reset section (CrOS only).
      if (cr.isChromeOS) {
        $('factory-reset-restart').onclick = function(event) {
          OptionsPage.navigateToPage('factoryResetData');
        };
      }

      // Kiosk section (CrOS only).
      if (cr.isChromeOS) {
        if (loadTimeData.getBoolean('enableKioskSection')) {
          $('kiosk-section').hidden = false;

          $('manage-kiosk-apps-button').onclick = function(event) {
            OptionsPage.navigateToPage('kioskAppsOverlay');
          };
        }
      }

      if (loadTimeData.getBoolean('managedUsersEnabled') &&
          loadTimeData.getBoolean('profileIsManaged')) {
        $('managed-user-settings-section').hidden = false;

        $('open-managed-user-settings-button').onclick = function(event) {
          OptionsPage.navigateToPage('managedUser');
        };
      }
    },

    /** @override */
    didShowPage: function() {
      $('search-field').focus();
    },

   /**
    * Called after all C++ UI handlers have called InitializePage to notify
    * that initialization is complete.
    * @private
    */
    notifyInitializationComplete_: function() {
      this.initializationComplete_ = true;
      cr.dispatchSimpleEvent(document, 'initializationComplete');
    },

    /**
     * Event listener for the 'session.restore_on_startup' pref.
     * @param {Event} event The preference change event.
     * @private
     */
    onRestoreOnStartupChanged_: function(event) {
      /** @const */ var showHomePageValue = 0;

      if (event.value.value == showHomePageValue) {
        // If the user previously selected "Show the homepage", the
        // preference will already be migrated to "Open a specific page". So
        // the only way to reach this code is if the 'restore on startup'
        // preference is managed.
        assert(event.value.controlledBy);

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
     * Shows the given section.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @param {boolean} animate Indicate if the expansion should be animated.
     * @private
     */
    showSection_: function(section, container, animate) {
      if (animate)
        this.addTransitionEndListener_(section);

      // Unhide
      section.hidden = false;

      var expander = function() {
        // Reveal the section using a WebKit transition if animating.
        if (animate) {
          section.classList.add('sliding');
          section.style.height = container.offsetHeight + 'px';
        } else {
          section.style.height = 'auto';
        }
        // Force an update of the list of paired Bluetooth devices.
        if (cr.isChromeOS)
          $('bluetooth-paired-devices-list').refresh();
      };

      // Delay starting the transition if animating so that hidden change will
      // be processed.
      if (animate)
        setTimeout(expander, 0);
      else
        expander();
      },

    /**
     * Shows the given section, with animation.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @private
     */
    showSectionWithAnimation_: function(section, container) {
      this.showSection_(section, container, /*animate */ true);
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
     * Scrolls the settings page to make the section visible auto-expanding
     * advanced settings if required.  The transition is not animated.  This
     * method is used to ensure that a section associated with an overlay
     * is visible when the overlay is closed.
     * @param {!Element} section  The section to make visible.
     * @private
     */
    scrollToSection_: function(section) {
      var advancedSettings = $('advanced-settings');
      var container = $('advanced-settings-container');
      if (advancedSettings.hidden && section.parentNode == container) {
        this.showSection_($('advanced-settings'),
                          $('advanced-settings-container'),
                          /* animate */ false);
        this.updateAdvancedSettingsExpander_();
      }

      if (!this.initializationComplete_) {
        var self = this;
        var callback = function() {
           document.removeEventListener('initializationComplete', callback);
           self.scrollToSection_(section);
        };
        document.addEventListener('initializationComplete', callback);
        return;
      }

      var pageContainer = $('page-container');
      var pageTop = parseFloat(pageContainer.style.top);
      var topSection = document.querySelector('#page-container section');
      var pageHeight = document.body.scrollHeight - topSection.offsetTop;
      var sectionTop = section.offsetTop;
      var sectionHeight = section.offsetHeight;
      var marginBottom = window.getComputedStyle(section).marginBottom;
      if (marginBottom)
        sectionHeight += parseFloat(marginBottom);
      if (pageHeight - pageTop < sectionTop + sectionHeight) {
        pageContainer.oldScrollTop = sectionTop + sectionHeight - pageHeight;
        var verticalPosition = pageContainer.getBoundingClientRect().top -
            pageContainer.oldScrollTop;
        pageContainer.style.top = verticalPosition + 'px';
      }
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
        expander.textContent = loadTimeData.getString('showAdvancedSettings');
      else
        expander.textContent = loadTimeData.getString('hideAdvancedSettings');
    },

    /**
     * Updates the sync section with the given state.
     * @param {Object} syncData A bunch of data records that describe the status
     *     of the sync system.
     * @private
     */
    updateSyncState_: function(syncData) {
      if (!syncData.signinAllowed) {
        $('sync-section').hidden = true;
        return;
      }

      $('sync-section').hidden = false;
      this.signedIn = syncData.signedIn;
      // Display the "setup sync" button if we're signed in and sync is not
      // managed/disabled.
      $('customize-sync').hidden = !syncData.signedIn ||
          syncData.managed || !syncData.syncSystemEnabled;

      var startStopButton = $('start-stop-sync');
      // Disable the "start/stop syncing" if we're currently signing in, or
      // if we're already signed in and signout is not allowed.
      startStopButton.disabled = syncData.setupInProgress ||
          !syncData.signoutAllowed;
      if (!syncData.signoutAllowed)
        $('start-stop-sync-indicator').setAttribute('controlled-by', 'policy');
      else
        $('start-stop-sync-indicator').removeAttribute('controlled-by');
      startStopButton.hidden =
          syncData.setupCompleted && cr.isChromeOS;
      startStopButton.textContent =
          syncData.signedIn ?
              loadTimeData.getString('syncButtonTextStop') :
          syncData.setupInProgress ?
              loadTimeData.getString('syncButtonTextInProgress') :
              loadTimeData.getString('syncButtonTextStart');
      $('start-stop-sync-indicator').hidden = startStopButton.hidden;

      // TODO(estade): can this just be textContent?
      $('sync-status-text').innerHTML = syncData.statusText;
      var statusSet = syncData.statusText.length != 0;
      $('sync-overview').hidden = statusSet;
      $('sync-status').hidden = !statusSet;

      $('sync-action-link').textContent = syncData.actionLinkText;
      // Don't show the action link if it is empty or undefined.
      $('sync-action-link').hidden = syncData.actionLinkText.length == 0;
      $('sync-action-link').disabled = syncData.managed ||
                                       !syncData.syncSystemEnabled;

      // On Chrome OS, sign out the user and sign in again to get fresh
      // credentials on auth errors.
      $('sync-action-link').onclick = function(event) {
        if (cr.isChromeOS && syncData.hasError)
          SyncSetupOverlay.doSignOutOnAuthError();
        else
          SyncSetupOverlay.showErrorUI();
      };

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
        if (event.value.value)
          this.showSectionWithAnimation_(section, container);
        else
          this.hideSectionWithAnimation_(section, container);
      } else {
        section.hidden = !event.value.value;
        this.onShowHomeButtonChangedCalled_ = true;
      }
    },

    /**
     * Event listener for the 'homepage is NTP' preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageIsNtpChanged_: function(event) {
      if (!event.value.uncommitted) {
        $('home-page-url').hidden = event.value.value;
        $('home-page-ntp').hidden = !event.value.value;
      }
    },

    /**
     * Event listener for changes to the homepage preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageChanged_: function(event) {
      if (!event.value.uncommitted)
        $('home-page-url').textContent = this.stripHttp_(event.value.value);
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
     * Called when the value of the download.default_directory preference
     * changes.
     * @param {Event} event Change event.
     * @private
     */
    onDefaultDownloadDirectoryChanged_: function(event) {
      $('downloadLocationPath').value = event.value.value;
      if (cr.isChromeOS) {
        // On ChromeOS, replace /special/drive with Drive for drive paths, and
        // /home/chronos/user/Downloads with Downloads for local files.
        // Also replace '/' with ' \u203a ' (angled quote sign) everywhere.
        var path = $('downloadLocationPath').value;
        path = path.replace(/^\/special\/drive/, 'Google Drive');
        path = path.replace(/^\/home\/chronos\/user\//, '');
        path = path.replace(/\//g, ' \u203a ');
        $('downloadLocationPath').value = path;
      }
      if (event.value.disabled)
        $('download-location-label').classList.add('disabled');
      else
        $('download-location-label').classList.remove('disabled');
      $('downloadLocationChangeButton').disabled = event.value.disabled;
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
      if (defaultManaged && defaultValue == -1)
        return;
      engineCount = engines.length;
      var defaultIndex = -1;
      for (var i = 0; i < engineCount; i++) {
        var engine = engines[i];
        var option = new Option(engine.name, engine.index);
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
      var isManaged = loadTimeData.getBoolean('profileIsManaged');
      $('profiles-manage').disabled = !hasSelection ||
          !selectedProfile.isCurrentProfile;
      if (hasSelection && !selectedProfile.isCurrentProfile)
        $('profiles-manage').title = loadTimeData.getString('currentUserOnly');
      else
        $('profiles-manage').title = '';
      $('profiles-delete').disabled = isManaged ||
                                      (!hasSelection && !hasSingleProfile);
      if (OptionsPage.isSettingsApp()) {
        $('profiles-app-list-switch').disabled = !hasSelection ||
            selectedProfile.isCurrentProfile;
      }
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
      $('profiles-manage').hidden =
          hasSingleProfile || OptionsPage.isSettingsApp();
      $('profiles-delete').textContent = hasSingleProfile ?
          loadTimeData.getString('profilesDeleteSingle') :
          loadTimeData.getString('profilesDelete');
      if (OptionsPage.isSettingsApp())
        $('profiles-app-list-switch').hidden = hasSingleProfile;
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

      // Received new data. If showing the "manage" overlay, keep it up to
      // date. If showing the "delete" overlay, close it.
      if (ManageProfileOverlay.getInstance().visible &&
          !$('manage-profile-overlay-manage').hidden) {
        ManageProfileOverlay.showManageDialog();
      } else {
        ManageProfileOverlay.getInstance().visible = false;
      }

      this.setProfileViewButtonsStatus_();
    },

    /**
     * Returns the currently active profile for this browser window.
     * @return {Object} A profile info object.
     * @private
     */
    getCurrentProfile_: function() {
      for (var i = 0; i < $('profiles-list').dataModel.length; i++) {
        var profile = $('profiles-list').dataModel.item(i);
        if (profile.isCurrentProfile)
          return profile;
      }

      assert(false,
             'There should always be a current profile, but none found.');
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
     * Enables factory reset section.
     * @private
     */
    enableFactoryResetSection_: function() {
      $('factory-reset-section').hidden = false;
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
     * Set the visibility of the password generation checkbox.
     * @private
     */
    setPasswordGenerationSettingVisibility_: function(visible) {
      if (visible)
        $('password-generation-checkbox').style.display = 'block';
      else
        $('password-generation-checkbox').style.display = 'none';
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
        var option = new Option(loadTimeData.getString('fontSizeLabelCustom'),
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
    setupProxySettingsSection_: function(disabled, extensionControlled) {
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').disabled = disabled;
        $('proxiesLabel').textContent =
            loadTimeData.getString(extensionControlled ?
                'proxiesLabelExtension' : 'proxiesLabelSystem');
      }
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
            loadTimeData.getString('cloudPrintConnectorDisabledButton');
          $('cloudPrintManageButton').style.display = 'none';
        } else {
          $('cloudPrintConnectorSetupButton').textContent =
            loadTimeData.getString('cloudPrintConnectorEnabledButton');
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
      $('accessibility-high-contrast-check').checked = checked;
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
     * Show/hide touchpad-related settings.
     * @private
     */
    showTouchpadControls_: function(show) {
      $('touchpad-settings').hidden = !show;
      $('accessibility-tap-dragging').hidden = !show;
    },

    /**
     * Show/hide the display options button on the System settings page.
     * @private
     */
    showDisplayOptions_: function(show) {
      $('display-options-section').hidden = !show;
    },

    /**
     * Activate the Bluetooth settings section on the System settings page.
     * @private
     */
    showBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = false;
    },

    /**
     * Dectivates the Bluetooth settings section from the System settings page.
     * @private
     */
    hideBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = true;
    },

    /**
     * Sets the state of the checkbox indicating if Bluetooth is turned on. The
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
     * Adds an element to the list of available Bluetooth devices. If an element
     * with a matching address is found, the existing element is updated.
     * @param {{name: string,
     *          address: string,
     *          paired: boolean,
     *          bonded: boolean,
     *          connected: boolean}} device
     *     Decription of the Bluetooth device.
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
      } else {
        // Test to see if the device is currently in the paired list, in which
        // case it should be removed from that list.
        var index = $('bluetooth-paired-devices-list').find(device.address);
        if (index != undefined)
          $('bluetooth-paired-devices-list').deleteItemAtIndex(index);
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
    }
  };

  //Forward public APIs to private implementations.
  [
    'addBluetoothDevice',
    'enableFactoryResetSection',
    'getCurrentProfile',
    'getStartStopSyncButton',
    'hideBluetoothSettings',
    'notifyInitializationComplete',
    'removeBluetoothDevice',
    'removeCloudPrintConnectorSection',
    'scrollToSection',
    'setAutoOpenFileTypesDisplayed',
    'setBluetoothState',
    'setFontSize',
    'setGtkThemeButtonEnabled',
    'setHighContrastCheckboxState',
    'setMetricsReportingCheckboxState',
    'setMetricsReportingSettingVisibility',
    'setPasswordGenerationSettingVisibility',
    'setProfilesInfo',
    'setSpokenFeedbackCheckboxState',
    'setThemesResetButtonEnabled',
    'setVirtualKeyboardCheckboxState',
    'setupCloudPrintConnectorSection',
    'setupPageZoomSelector',
    'setupProxySettingsSection',
    'showBluetoothSettings',
    'showDisplayOptions',
    'showMouseControls',
    'showTouchpadControls',
    'updateAccountPicture',
    'updateAutoLaunchState',
    'updateDefaultBrowserState',
    'updateSearchEngines',
    'updateStartupPages',
    'updateSyncState',
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
