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

    showOverlay_: function() {
      $('sync-setup-overlay').hidden = false;
    },

    initializePage: function() {
      options.SyncSetupOverlay.prototype.initializePage.call(this);
      chrome.send('SyncSetupAttachHandler');
    },

    showOverlay_: function() {
      $('sync-setup-overlay').hidden = false;
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

  // Export
  return {
    NewTabSyncPromo : NewTabSyncPromo
  };
});

var OptionsPage = options.OptionsPage;
var SyncSetupOverlay = new_tab.NewTabSyncPromo;
window.addEventListener('DOMContentLoaded', new_tab.NewTabSyncPromo.initialize);
