// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
    OptionsPage.call(this, 'personal', templateData.personalPage,
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
        chrome.send('showSyncLoginDialog');
      };
      $('start-stop-sync').onclick = function(event) {
        if (self.syncSetupCompleted)
          self.showStopSyncingOverlay_();
        else
          chrome.send('showSyncLoginDialog');
      };
      $('privacy-dashboard-link').onclick = function(event) {
        chrome.send('openPrivacyDashboardTabAndActivate');
      };
      $('manage-passwords').onclick = function(event) {
        OptionsPage.showPageByName('passwordManager');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordManager']);
      };
      $('autofill-settings').onclick = function(event) {
        OptionsPage.showPageByName('autoFillOptions');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutoFillSettings']);
      };
      $('themes-reset').onclick = function(event) {
        chrome.send('themesReset');
      };

      // Initialize sync select control.
      $('sync-select').onchange = function(event) {
        self.updateSyncSelection_();
      }

      var syncCheckboxes = $('sync-table').getElementsByTagName('input');
      for (var i = 0; i < syncCheckboxes.length; i++) {
        if (syncCheckboxes[i].type == "checkbox") {
          syncCheckboxes[i].onclick = function(event) {
            chrome.send('updatePreferredDataTypes');
          };
        }
      }

      if (!cr.isChromeOS) {
        $('import-data').onclick = function(event) {
          OptionsPage.showOverlay('importDataOverlay');
          chrome.send('coreOptionsUserMetricsAction', ['Import_ShowDlg']);
        };

        if (navigator.platform.match(/linux|BSD/i)) {
          $('themes-GTK-button').onclick = function(event) {
            chrome.send('themesSetGTK');
          };
        }
      } else {
        chrome.send('loadAccountPicture');
      }

      // Disable the screen lock checkbox for the guest mode.
      if (cr.commandLine.options['--bwsi'])
        $('enable-screen-lock').disabled = true;
    },

    /**
     * Updates the sync datatype checkboxes based on the selected sync option.
     * @private
     */
    updateSyncSelection_: function() {
      var idx = $('sync-select').selectedIndex;
      var syncCheckboxes = $('sync-table').getElementsByTagName('input');
      if (idx == 0) {
        // 'Choose what to sync.'
        for (var i = 0; i < syncCheckboxes.length; i++) {
          syncCheckboxes[i].disabled = false;
        }
      } else if (idx == 1) {
        // 'Everything' is synced.
        for (var i = 0; i < syncCheckboxes.length; i++) {
          if (!syncCheckboxes[i].checked) {
            syncCheckboxes[i].checked = true;

            // Merely setting checked = true is not enough to trigger the pref
            // being set; thus, we dispatch a change event to notify
            // PrefCheckbox |checked| has changed.
            cr.dispatchSimpleEvent(syncCheckboxes[i], 'change');
          }

          syncCheckboxes[i].disabled = true;
        }
      }
    },

    showStopSyncingOverlay_: function(event) {
      AlertOverlay.show(localStrings.getString('stop_syncing_title'),
                        localStrings.getString('stop_syncing_explanation'),
                        localStrings.getString('stop_syncing_confirm'),
                        localStrings.getString('cancel'),
                        function() { chrome.send('stopSyncing'); });
    },

    setElementVisible_: function(element, visible) {
      if (visible)
        element.classList.remove('hidden');
      else
        element.classList.add('hidden');
    },

    setElementClassSyncError_: function(element, visible) {
      visible ? element.classList.add('sync-error') :
                element.classList.remove('sync-error');
    },

    setSyncEnabled_: function(enabled) {
      this.syncEnabled = enabled;
    },

    setSyncSetupCompleted_: function(completed) {
      this.syncSetupCompleted = completed;
    },

    setAccountPicture_: function(image) {
      $('account-picture').src = image;
    },

    setSyncStatus_: function(status) {
      $('sync-status').textContent = status;
    },

    setSyncStatusErrorVisible_: function(visible) {
      this.setElementClassSyncError_($('sync-status'), visible);
    },

    setSyncActionLinkErrorVisible_: function(visible) {
      this.setElementClassSyncError_($('sync-action-link'), visible);
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

  // Forward public APIs to private implementations.
  [
    'setSyncEnabled',
    'setSyncSetupCompleted',
    'setAccountPicture',
    'setSyncStatus',
    'setSyncStatusErrorVisible',
    'setSyncActionLinkErrorVisible',
    'setSyncActionLinkEnabled',
    'setSyncActionLinkLabel',
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
