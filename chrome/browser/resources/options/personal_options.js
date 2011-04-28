// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  // State variables.
  var syncEnabled = false;
  var syncSetupCompleted = false;

  /**
   * Encapsulated handling of personal options page.
   * @constructor
   */
  function PersonalOptions() {
    OptionsPage.call(this, 'personal',
                     templateData.personalPageTabTitle,
                     'personal-page');
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
      $('sync-action-link').onclick = function(event) {
        chrome.send('showSyncActionDialog');
      };
      $('start-stop-sync').onclick = function(event) {
        if (self.syncSetupCompleted)
          self.showStopSyncingOverlay_();
        else
          chrome.send('showSyncLoginDialog');
      };
      $('customize-sync').onclick = function(event) {
        chrome.send('showCustomizeSyncDialog');
      };
      $('privacy-dashboard-link').onclick = function(event) {
        chrome.send('openPrivacyDashboardTabAndActivate');
      };
      $('manage-passwords').onclick = function(event) {
        OptionsPage.navigateToPage('passwords');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordManager']);
      };
      $('autofill-settings').onclick = function(event) {
        OptionsPage.navigateToPage('autofill');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutofillSettings']);
      };
      $('themes-reset').onclick = function(event) {
        chrome.send('themesReset');
      };

      if (!cr.isChromeOS) {
        $('import-data').onclick = function(event) {
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
        chrome.send('loadAccountPicture');
      }

      if (cr.commandLine.options['--bwsi']) {
        // Disable the screen lock checkbox for the guest mode.
        $('enable-screen-lock').disabled = true;
      }

      if (PersonalOptions.disablePasswordManagement()) {
        $('passwords-offersave').disabled = true;
        $('passwords-neversave').disabled = true;
        $('passwords-offersave').value = false;
        $('passwords-neversave').value = true;
        $('manage-passwords').disabled = true;
      }
    },

    showStopSyncingOverlay_: function() {
      AlertOverlay.show(localStrings.getString('stop_syncing_title'),
                        localStrings.getString('stop_syncing_explanation'),
                        localStrings.getString('stop_syncing_confirm'),
                        localStrings.getString('cancel'),
                        function() { chrome.send('stopSyncing'); });
    },

    setElementVisible_: function(element, visible) {
      element.hidden = !visible;
      if (visible)
        element.classList.remove('hidden');
      else
        element.classList.add('hidden');
    },

    setSyncEnabled_: function(enabled) {
      this.syncEnabled = enabled;
    },

    setSyncSetupCompleted_: function(completed) {
      this.syncSetupCompleted = completed;
      this.setElementVisible_($('customize-sync'), completed);
      $('privacy-dashboard-link').hidden = !completed;
    },

    setAccountPicture_: function(image) {
      $('account-picture').src = image;
    },

    setSyncStatus_: function(status) {
      var statusSet = status != '';
      $('sync-overview').hidden = statusSet;
      $('sync-status').hidden = !statusSet;
      $('sync-status-text').textContent = status;
    },

    setSyncStatusErrorVisible_: function(visible) {
      visible ? $('sync-status').classList.add('sync-error') :
                $('sync-status').classList.remove('sync-error');
    },

    setSyncActionLinkEnabled_: function(enabled) {
      $('sync-action-link').disabled = !enabled;
    },

    setSyncActionLinkLabel_: function(status) {
      $('sync-action-link').textContent = status;

      // link-button does is not zero-area when the contents of the button are
      // empty, so explicitly hide the element.
      this.setElementVisible_($('sync-action-link'), status.length != 0);
    },

    setProfilesSectionVisible_: function(visible) {
      this.setElementVisible_($('profiles-create'), visible);
    },

    setNewProfileButtonEnabled_: function(enabled) {
      $('new-profile').disabled = !enabled;
      if (enabled)
        $('profiles-create').classList.remove('disabled');
      else
        $('profiles-create').classList.add('disabled');
    },

    setStartStopButtonVisible_: function(visible) {
      this.setElementVisible_($('start-stop-sync'), visible);
    },

    setStartStopButtonEnabled_: function(enabled) {
      $('start-stop-sync').disabled = !enabled;
    },

    setStartStopButtonLabel_: function(label) {
      $('start-stop-sync').textContent = label;
    },

    setGtkThemeButtonEnabled_: function(enabled) {
      if (!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
        $('themes-GTK-button').disabled = !enabled;
      }
    },

    setThemesResetButtonEnabled_: function(enabled) {
      $('themes-reset').disabled = !enabled;
    },

    hideSyncSection_: function() {
      this.setElementVisible_($('sync-section'), false);
    },

    /**
     * Toggles the visibility of the data type checkboxes based on whether they
     * are enabled on not.
     * @param {Object} dict A mapping from data type to a boolean indicating
     *     whether it is enabled.
     * @private
     */
    setRegisteredDataTypes_: function(dict) {
      for (var type in dict) {
        if (type.match(/Registered$/) && !dict[type]) {
          node = $(type.replace(/([a-z]+)Registered$/i, '$1').toLowerCase()
                   + '-check');
          if (node)
            node.parentNode.style.display = 'none';
        }
      }
    },
  };

  /**
   * Returns whether the user should be able to manage (view and edit) their
   * stored passwords. Password management is disabled in guest mode.
   * @return {boolean} True if password management should be disabled.
   */
  PersonalOptions.disablePasswordManagement = function() {
    return cr.commandLine.options['--bwsi'];
  };

  // Forward public APIs to private implementations.
  [
    'setSyncEnabled',
    'setSyncSetupCompleted',
    'setAccountPicture',
    'setSyncStatus',
    'setSyncStatusErrorVisible',
    'setSyncActionLinkEnabled',
    'setSyncActionLinkLabel',
    'setProfilesSectionVisible',
    'setNewProfileButtonEnabled',
    'setStartStopButtonVisible',
    'setStartStopButtonEnabled',
    'setStartStopButtonLabel',
    'setGtkThemeButtonEnabled',
    'setThemesResetButtonEnabled',
    'hideSyncSection',
    'setRegisteredDataTypes',
  ].forEach(function(name) {
    PersonalOptions[name] = function(value) {
      PersonalOptions.getInstance()[name + '_'](value);
    };
  });

  // Export
  return {
    PersonalOptions: PersonalOptions
  };

});
