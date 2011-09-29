// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('sync_promo', function() {
  /**
   * SyncPromo class
   * Subclass of options.SyncSetupOverlay that customizes the sync setup
   * overlay for use in the new tab page.
   * @class
   */
  function SyncPromo() {
    options.SyncSetupOverlay.call(this, 'syncSetup',
                                  templateData.syncSetupOverlayTitle,
                                  'sync-setup-overlay');
  }

  cr.addSingletonGetter(SyncPromo);

  SyncPromo.prototype = {
    __proto__: options.SyncSetupOverlay.prototype,

    showOverlay_: function() {
      $('sync-setup-overlay').hidden = false;
    },

    closeOverlay_: function() {
      chrome.send('CloseSyncPromo');
    },

    // Initializes the page.
    initializePage: function() {
      options.SyncSetupOverlay.prototype.initializePage.call(this);

      // Hide parts of the login UI and show the promo UI.
      this.hideOuterLoginUI_();
      $('promo-title').hidden = false;
      $('sync-setup-login-promo-column').hidden = false;
      $('promo-skip').hidden = false;

      chrome.send('InitializeSyncPromo');

      var self = this;
      $('promo-skip-button').addEventListener('click', function() {
        self.closeOverlay_();
      });
    },
  };

  SyncPromo.showErrorUI = function() {
    SyncPromo.getInstance().showErrorUI_();
  };

  SyncPromo.showSetupUI = function() {
    SyncPromo.getInstance().showSetupUI_();
  };

  SyncPromo.showSyncSetupPage = function(page, args) {
    SyncPromo.getInstance().showSyncSetupPage_(page, args);
  };

  SyncPromo.showSuccessAndClose = function() {
    SyncPromo.getInstance().showSuccessAndClose_();
  };

  SyncPromo.showSuccessAndSettingUp = function() {
    SyncPromo.getInstance().showSuccessAndSettingUp_();
  };

  SyncPromo.showStopSyncingUI = function() {
    SyncPromo.getInstance().showStopSyncingUI_();
  };

  SyncPromo.initialize = function() {
    SyncPromo.getInstance().initializePage();
  }

  // Export
  return {
    SyncPromo : SyncPromo
  };
});

var OptionsPage = options.OptionsPage;
var SyncSetupOverlay = sync_promo.SyncPromo;
window.addEventListener('DOMContentLoaded', sync_promo.SyncPromo.initialize);
