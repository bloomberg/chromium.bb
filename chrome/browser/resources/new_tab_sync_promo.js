// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('new_tab', function() {
  /**
   * NewTabSyncPromo class
   * Subclass of options.SyncSetupOverlay that customizes the sync setup
   * overlay for use in the new tab page.
   * @class
   */
  function NewTabSyncPromo() {
    options.SyncSetupOverlay.call(this, 'syncSetup',
                                  templateData.syncSetupOverlayTitle,
                                  'sync-setup-overlay');
  }

  cr.addSingletonGetter(NewTabSyncPromo);

  NewTabSyncPromo.prototype = {
    __proto__: options.SyncSetupOverlay.prototype,

    // Variable to track if the promo is expanded or collapsed.
    isSyncPromoExpanded_: false,

    showOverlay_: function() {
      this.expandSyncPromo_(true);
    },

    // Initializes the page.
    initializePage: function() {
      options.SyncSetupOverlay.prototype.initializePage.call(this);
      var self = this;
      $('sync-promo-toggle-button').onclick = function() {
        self.onTogglePromo();
      }
      chrome.send('InitializeSyncPromo');
    },

    // Handler for the toggle button to show or hide the sync promo.
    onTogglePromo: function() {
      if (this.isSyncPromoExpanded_) {
        this.expandSyncPromo_(false);
        chrome.send('CollapseSyncPromo');
      } else {
        chrome.send('ExpandSyncPromo');
      }
    },

    // Shows or hides the sync promo.
    expandSyncPromo_: function(shouldExpand) {
      this.isSyncPromoExpanded_ = shouldExpand;
      if (shouldExpand) {
        $('sync-promo-login-status').hidden = true;
        $('sync-setup-overlay').hidden = false;
        $('sync-promo').classList.remove('collapsed');
      } else {
        $('sync-promo-login-status').hidden = false;
        $('sync-setup-overlay').hidden = true;
        $('sync-promo').classList.add('collapsed');
      }
      layoutSections();
    },

    // Sets the sync login name. If there's no login name then makes the
    // 'not connected' UI visible and shows the sync promo toggle button.
    updateLogin_: function(user_name) {
      if (user_name) {
        $('sync-promo-toggle').hidden = true;
        $('sync-promo-user-name').textContent = user_name;
        $('sync-promo-not-connected').hidden = true;
      } else {
        $('sync-promo-toggle').hidden = false;
        $('sync-promo-user-name').hidden = true;
        $('sync-promo-not-connected').hidden = false;
      }
      layoutSections();
    },

    // Shows the sync promo.
    showSyncPromo_: function() {
      $('sync-promo').hidden = false;
    },
  };

  NewTabSyncPromo.showErrorUI = function() {
    NewTabSyncPromo.getInstance().showErrorUI_();
  };

  NewTabSyncPromo.showSetupUI = function() {
    NewTabSyncPromo.getInstance().showSetupUI_();
  };

  NewTabSyncPromo.showSyncSetupPage = function(page, args) {
    NewTabSyncPromo.getInstance().showSyncSetupPage_(page, args);
  };

  NewTabSyncPromo.showSuccessAndClose = function() {
    NewTabSyncPromo.getInstance().showSuccessAndClose_();
  };

  NewTabSyncPromo.showSuccessAndSettingUp = function() {
    NewTabSyncPromo.getInstance().showSuccessAndSettingUp_();
  };

  NewTabSyncPromo.showStopSyncingUI = function() {
    NewTabSyncPromo.getInstance().showStopSyncingUI_();
  };

  NewTabSyncPromo.initialize = function() {
    NewTabSyncPromo.getInstance().initializePage();
  }

  NewTabSyncPromo.showSyncPromo = function() {
    NewTabSyncPromo.getInstance().showSyncPromo_();
  }

  NewTabSyncPromo.updateLogin = function(user_name) {
    NewTabSyncPromo.getInstance().updateLogin_(user_name);
  }

  // Export
  return {
    NewTabSyncPromo : NewTabSyncPromo
  };
});

var OptionsPage = options.OptionsPage;
var SyncSetupOverlay = new_tab.NewTabSyncPromo;
window.addEventListener('DOMContentLoaded', new_tab.NewTabSyncPromo.initialize);
