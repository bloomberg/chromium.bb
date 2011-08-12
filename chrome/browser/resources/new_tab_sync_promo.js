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

    // Variable to track if the login name should be clickable.
    isLoginNameClickable_ : false,

    // Variable to track if the user is signed into sync.
    isUserSignedIntoSync_ : false,

    // Override SyncSetupOverlay::showOverlay_ to expand the sync promo when the
    // overlay is shown.
    showOverlay_: function() {
      this.isSyncPromoExpanded_ = true;
      $('sync-promo-login-status').hidden = true;
      $('sync-setup-overlay').hidden = false;
      $('sync-promo').classList.remove('collapsed');
      layoutSections();
    },

    // Override SyncSetupOverlay::closeOverlay_ to collapse the sync promo when
    // the overlay is closed.
    closeOverlay_: function() {
      options.SyncSetupOverlay.prototype.closeOverlay_.call(this);
      this.isSyncPromoExpanded_ = false;
      $('sync-promo-login-status').hidden = false;
      $('sync-setup-overlay').hidden = true;
      $('sync-promo').classList.add('collapsed');
      layoutSections();

      // If the overlay is being closed because the user pressed cancelled
      // then we need to ensure that chrome knows about it.
      chrome.send('CollapseSyncPromo');
    },

    // Initializes the page.
    initializePage: function() {
      options.SyncSetupOverlay.prototype.initializePage.call(this);
      var self = this;
      $('sync-promo-toggle-button').onclick = function() {
        self.onTogglePromo();
      }
      $('sync-promo-login-status-cell').onclick = function() {
        self.onLoginNameClicked();
      }
      chrome.send('InitializeSyncPromo');
    },

    // Handler for the toggle button to show or hide the sync promo.
    onTogglePromo: function() {
      if (this.isSyncPromoExpanded_) {
        chrome.send('CollapseSyncPromo');
        this.closeOverlay_();
      } else {
        chrome.send('ExpandSyncPromo');
      }
    },

    // Called when the user clicks the login name on the top right of the sync
    // promo. If the user isn't signed in then we just expand the sync promo.
    // If the user is signed in then we ask Chrome to show the profile menu.
    onLoginNameClicked: function() {
      if (!this.isLoginNameClickable_)
        return;

      if (this.isUserSignedIntoSync_)
        chrome.send('ShowProfileMenu');
      else
        this.onTogglePromo();
    },

    // Updates the sync login status. If the user is not logged in then it
    // shows the sync promo toggle button.
    updateLogin_: function(login_status_msg, icon_url, is_signed_in,
                           is_clickable) {
      $('sync-promo-login-status-msg').textContent = login_status_msg;
      this.isUserSignedIntoSync_ = is_signed_in;
      $('sync-promo-toggle').hidden = is_signed_in;

      if (icon_url == "") {
        $('login-status-avatar-icon').hidden = true;
      } else {
        $('login-status-avatar-icon').hidden = false;
        $('login-status-avatar-icon').src = icon_url;
      }

      this.isLoginNameClickable_ = is_clickable;
      if (is_clickable)
        $('sync-promo-login-status-cell').style.cursor = "pointer";
      else
        $('sync-promo-login-status-cell').style.cursor = "default";

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

  NewTabSyncPromo.updateLogin = function(user_name, icon_url, is_signed_in,
                                         is_clickable) {
    NewTabSyncPromo.getInstance().updateLogin_(
        user_name, icon_url, is_signed_in, is_clickable);
  }

  // Export
  return {
    NewTabSyncPromo : NewTabSyncPromo
  };
});

var OptionsPage = options.OptionsPage;
var SyncSetupOverlay = new_tab.NewTabSyncPromo;
window.addEventListener('DOMContentLoaded', new_tab.NewTabSyncPromo.initialize);
