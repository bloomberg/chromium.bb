// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// PersonalOptions class
// Encapsulated handling of personal options page.
//
function PersonalOptions() {
  OptionsPage.call(this, 'personal', templateData.personalPage, 'personalPage');
}

cr.addSingletonGetter(PersonalOptions);

PersonalOptions.prototype = {
  // Inherit PersonalOptions from OptionsPage.
  __proto__: OptionsPage.prototype,

  // Initialize PersonalOptions page.
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // Listen to pref changes.
    Preferences.getInstance().addEventListener('sync.has_setup_completed',
        function(event) {
          if(event.value) {
            chrome.send('getSyncStatus');
            $('text-when-synced').style.display = 'block';
            $('button-when-synced').style.display = 'block';
            $('stop-sync').onclick = function(event) {
              OptionsPage.showOverlay('stopSyncingOverlay');
            };

            $('sync-customize').onclick = function(event) {
              OptionsPage.showPageByName('sync');
            };

            $('text-when-not-synced').style.display = 'none';
            $('button-when-not-synced').style.display = 'none';
          }
          else {
            $('text-when-not-synced').style.display = 'block';
            $('button-when-not-synced').style.display = 'block';
            $('start-sync').onclick = function(event) {
              //TODO(sargrass): Show start-sync subpage, after dhg done.
            };

            $('text-when-synced').style.display = 'none';
            $('button-when-synced').style.display = 'none';
          }
        });


    $('showpasswords').onclick = function(event) {
      //TODO(sargrass): Show passwords dialog here.
    };

    $('autofill_options').onclick = function(event) {
      //TODO(sargrass): Show autofill dialog here.
    };

    $('import_data').onclick = function(event) {
        OptionsPage.showOverlay('importDataOverlay');
    };

    if(!cr.isChromeOS && navigator.platform.match(/linux|BSD/i)) {
      $('themes_GTK_button').onclick = function(event) {
        //TODO(sargrass): Show themes GTK dialog here.
      };

      $('themes_set_classic').onclick = function(event) {
        //TODO(sargrass): Show themes set classic dialog here.
      };
    }

    if(cr.isMac || cr.isWindows || cr.isChromeOS) {
      $('themes_reset').onclick = function(event) {
        //TODO(sargrass): Show themes reset dialog here.
      };
    }
  },

  syncStatusCallback_: function(statusString) {
    $('synced_to_user_with_time').textContent = statusString;
  },
};

PersonalOptions.syncStatusCallback = function(statusString){
  PersonalOptions.getInstance().syncStatusCallback_(statusString);
};
