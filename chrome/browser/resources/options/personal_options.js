// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

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

      $('sync-customize').onclick = function(event) {
        OptionsPage.showPageByName('sync');
      };
      $('start-sync').onclick = function(event) {
        //TODO(sargrass): Show start-sync subpage, after dhg done.
      };

      // Listen to pref changes.
      Preferences.getInstance().addEventListener('sync.has_setup_completed',
          function(event) {
            if(event.value) {
              chrome.send('getSyncStatus');
              $('synced-controls').classList.remove('hidden');
              $('not-synced-controls').classList.add('hidden');
            } else {
              $('synced-controls').classList.add('hidden');
              $('not-synced-controls').classList.remove('hidden');
            }
          });

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
        $('stop-sync').onclick = function(event) {
          AlertOverlay.show(localStrings.getString('stop_syncing_title'),
              localStrings.getString('stop_syncing_explanation'),
              localStrings.getString('stop_syncing_confirm_button_label'),
              undefined,
              function() { chrome.send('stopSyncing'); });
        };
        $('import_data').onclick = function(event) {
          OptionsPage.showOverlay('importDataOverlay');
          chrome.send('coreOptionsUserMetricsAction', ['Import_ShowDlg']);
        };
      }

      if(!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
        $('themes_GTK_button').onclick = function(event) {
          chrome.send('themesSetGTK');
        };

        $('themes_set_classic').onclick = function(event) {
          chrome.send('themesReset');
        };
        $('themes-gallery').onclick = function(event) {
          chrome.send('themesGallery');
        }
      }

      if(cr.isMac || cr.isWindows || cr.isChromeOS) {
        $('themes_reset').onclick = function(event) {
          chrome.send('themesReset');
        };
        $('themes-gallery').onclick = function(event) {
          chrome.send('themesGallery');
        }
      }
    },

    syncStatusCallback_: function(statusString) {
      $('synced_to_user_with_time').textContent = statusString;
    },
  };

  PersonalOptions.syncStatusCallback = function(statusString) {
    PersonalOptions.getInstance().syncStatusCallback_(statusString);
  };

  // Export
  return {
    PersonalOptions: PersonalOptions
  };

});

