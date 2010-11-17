// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  // State variables.
  var syncEnabled = false;
  var syncSetupCompleted = false;

  //
  // PersonalOptions class
  // Encapsulated handling of personal options page.
  //
  function PersonalOptions() {
    OptionsPage.call(this, 'personal', templateData.personalPage,
                     'personalPage');
  }

  cr.addSingletonGetter(PersonalOptions);

  PersonalOptions.prototype = {
    // Inherit PersonalOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    // Initialize PersonalOptions page.
    initializePage: function() {
      // Call base class implementation to starts preference initialization.
      OptionsPage.prototype.initializePage.call(this);

      var self = this;
      $('customize-sync').onclick = function(event) {
        OptionsPage.showPageByName('sync');
      };
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
      $('showpasswords').onclick = function(event) {
        PasswordsExceptions.load();
        OptionsPage.showPageByName('passwordsExceptions');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordsExceptions']);
      };
      $('autofill_options').onclick = function(event) {
        OptionsPage.showPageByName('autoFillOptions');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutoFillSettings']);
      };

      if (!cr.isChromeOS) {
        $('import_data').onclick = function(event) {
          OptionsPage.showOverlay('importDataOverlay');
          chrome.send('coreOptionsUserMetricsAction', ['Import_ShowDlg']);
        };
      }

      if (!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
        $('themes_GTK_button').onclick = function(event) {
          chrome.send('themesSetGTK');
        };
        $('themes_set_classic').onclick = function(event) {
          chrome.send('themesReset');
        };
      }

      if (cr.isMac || cr.isWindows || cr.isChromeOS) {
        $('themes_reset').onclick = function(event) {
          chrome.send('themesReset');
        };
      }

      if (cr.isChromeOS) {
        chrome.send('loadAccountPicture');
      }
    },

    showStopSyncingOverlay_: function(event) {
      AlertOverlay.show(
          localStrings.getString('stop_syncing_title'),
          localStrings.getString('stop_syncing_explanation'),
          localStrings.getString('stop_syncing_confirm_button_label'),
          undefined,
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

    setCustomizeButtonVisible_: function(visible) {
      this.setElementVisible_($('customize-sync'), visible);
    },

    setCustomizeButtonEnabled_: function(enabled) {
      $('customize-sync').disabled = !enabled;
    },

    setCustomizeButtonLabel_: function(label) {
      $('customize-sync').textContent = label;
    },

    setGtkThemeButtonEnabled_: function(enabled) {
      if (!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
        $('themes_GTK_button').disabled = !enabled;
      }
    },

    setClassicThemeButtonEnabled_: function(enabled) {
      if (!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
        $('themes_set_classic').disabled = !enabled;
      }
    },

    hideSyncSection_: function() {
      this.setElementVisible_($('sync-section'), false);
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
    'setCustomizeButtonVisible',
    'setCustomizeButtonEnabled',
    'setCustomizeButtonLabel',
    'setGtkThemeButtonEnabled',
    'setClassicThemeButtonEnabled',
    'hideSyncSection',
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
