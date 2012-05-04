// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sail): Refactor options_page and remove this include.
<include src="../options2/options_page.js"/>
<include src="../shared/js/util.js"/>
<include src="../sync_setup_overlay.js"/>

cr.define('sync_promo', function() {
  /**
   * SyncPromo class
   * Subclass of options.SyncSetupOverlay that customizes the sync setup
   * overlay for use in the new tab page.
   * @class
   */
  function SyncPromo() {
    options.SyncSetupOverlay.call(this, 'syncSetup',
                                  templateData.syncSetupOverlayTabTitle,
                                  'sync-setup-overlay');
  }

  // Replicating enum from chrome/common/extensions/extension_constants.h.
  /** @const */ var actions = (function() {
    var i = 0;
    return {
      VIEWED: i++,
      LEARN_MORE_CLICKED: i++,
      ACCOUNT_HELP_CLICKED: i++,
      CREATE_ACCOUNT_CLICKED: i++,
      SKIP_CLICKED: i++,
      SIGN_IN_ATTEMPTED: i++,
      SIGNED_IN_SUCCESSFULLY: i++,
      ADVANCED_CLICKED: i++,
      ENCRYPTION_HELP_CLICKED: i++,
      CANCELLED_AFTER_SIGN_IN: i++,
      CONFIRMED_AFTER_SIGN_IN: i++,
      CLOSED_TAB: i++,
      CLOSED_WINDOW: i++,
      LEFT_DURING_THROBBER: i++,
    };
  }());

  cr.addSingletonGetter(SyncPromo);

  SyncPromo.prototype = {
    __proto__: options.SyncSetupOverlay.prototype,

    showOverlay_: function() {
      $('sync-setup-overlay').hidden = false;
    },

    closeOverlay_: function() {
      chrome.send('SyncPromo:Close');
    },

    // Initializes the page.
    initializePage: function() {
      localStrings = new LocalStrings();

      options.SyncSetupOverlay.prototype.initializePage.call(this);

      // Hide parts of the login UI and show the promo UI.
      this.hideOuterLoginUI_();
      $('promo-skip').hidden = false;

      this.showSetupUI_();
      chrome.send('SyncPromo:Initialize');

      var self = this;

      $('promo-skip-button').addEventListener('click', function() {
        chrome.send('SyncPromo:UserSkipped');
        self.closeOverlay_();
      });

      var learnMoreClickedAlready = false;
      $('promo-learn-more').addEventListener('click', function() {
        if (!learnMoreClickedAlready)
          chrome.send('SyncPromo:UserFlowAction', [actions.LEARN_MORE_CLICKED]);
        learnMoreClickedAlready = true;
      });

      $('promo-advanced').addEventListener('click', function() {
        chrome.send('SyncPromo:ShowAdvancedSettings');
      });

      var accountHelpClickedAlready = false;
      $('cannot-access-account-link').addEventListener('click', function() {
        if (!accountHelpClickedAlready)
          chrome.send('SyncPromo:UserFlowAction',
                      [actions.ACCOUNT_HELP_CLICKED]);
        accountHelpClickedAlready = true;
      });

      var createAccountClickedAlready = false;
      $('create-account-link').addEventListener('click', function() {
        if (!createAccountClickedAlready)
          chrome.send('SyncPromo:UserFlowAction',
                      [actions.CREATE_ACCOUNT_CLICKED]);
        createAccountClickedAlready = true;
      });

      // We listen to the <form>'s submit vs. the <input type="submit"> click so
      // we also track users that use the keyboard and press enter.
      var signInAttemptedAlready = false;
      $('gaia-login-form').addEventListener('submit', function() {
        ++self.signInAttempts_;
        if (!signInAttemptedAlready)
          chrome.send('SyncPromo:UserFlowAction', [actions.SIGN_IN_ATTEMPTED]);
        signInAttemptedAlready = true;
      });

      var encryptionHelpClickedAlready = false;
      $('encryption-help-link').addEventListener('click', function() {
        if (!encryptionHelpClickedAlready)
          chrome.send('SyncPromo:UserFlowAction',
                      [actions.ENCRYPTION_HELP_CLICKED]);
        encryptionHelpClickedAlready = true;
      });

      var advancedOptionsClickedAlready = false;
      $('customize-link').addEventListener('click', function() {
        if (!advancedOptionsClickedAlready)
          chrome.send('SyncPromo:UserFlowAction', [actions.ADVANCED_CLICKED]);
        advancedOptionsClickedAlready = true;
      });

      // Re-used across both cancel buttons after a successful sign in.
      var cancelFunc = function() {
        chrome.send('SyncPromo:UserFlowAction',
                    [actions.CANCELLED_AFTER_SIGN_IN]);
      };
      $('confirm-everything-cancel').addEventListener('click', cancelFunc);
      $('choose-datatypes-cancel').addEventListener('click', cancelFunc);

      // Add the source parameter to the document so that it can be used to
      // selectively show and hide elements using CSS.
      var params = parseQueryParams(document.location);
      if (params.source == '0')
        document.documentElement.setAttribute('isstartup', '');
    },

    /**
     * Called when the page is unloading to record number of times a user tried
     * to sign in and if they left while a throbber was running.
     * @private
     */
    recordPageViewActions_: function() {
      chrome.send('SyncPromo:RecordSignInAttempts', [this.signInAttempts_]);
      if (this.throbberStart_)
        chrome.send('SyncPromo:UserFlowAction', [actions.LEFT_DURING_THROBBER]);
    },

    /** @inheritDoc */
    sendConfiguration_: function() {
      chrome.send('SyncPromo:UserFlowAction',
                  [actions.CONFIRMED_AFTER_SIGN_IN]);
      options.SyncSetupOverlay.prototype.sendConfiguration_.apply(this,
          arguments);
    },

    /** @inheritDoc */
    setThrobbersVisible_: function(visible) {
      if (visible) {
        this.throbberStart_ = Date.now();
      } else {
        if (this.throbberStart_) {
          chrome.send('SyncPromo:RecordThrobberTime',
                      [Date.now() - this.throbberStart_]);
        }
        this.throbberStart_ = 0;
      }
      // Pass through to SyncSetupOverlay to handle display logic.
      options.SyncSetupOverlay.prototype.setThrobbersVisible_.apply(
          this, arguments);
    },

    /**
     * Number of times a user attempted to sign in to GAIA during this page
     * view.
     * @private
     */
    signInAttempts_: 0,

    /**
     * The start time of a throbber on the page.
     * @private
     */
    throbberStart_: 0,
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
    chrome.send('SyncPromo:UserFlowAction', [actions.SIGNED_IN_SUCCESSFULLY]);
    SyncPromo.getInstance().showSuccessAndSettingUp_();
  };

  SyncPromo.showStopSyncingUI = function() {
    SyncPromo.getInstance().showStopSyncingUI_();
  };

  SyncPromo.initialize = function() {
    SyncPromo.getInstance().initializePage();
  };

  SyncPromo.recordPageViewActions = function() {
    SyncPromo.getInstance().recordPageViewActions_();
  };

  SyncPromo.populatePromoMessage = function(resName) {
    SyncPromo.getInstance().populatePromoMessage_(resName);
  };

  // Export
  return {
    SyncPromo: SyncPromo
  };
});

var OptionsPage = options.OptionsPage;
var SyncSetupOverlay = sync_promo.SyncPromo;
window.addEventListener('DOMContentLoaded', sync_promo.SyncPromo.initialize);
window.addEventListener('beforeunload',
    sync_promo.SyncPromo.recordPageViewActions.bind(sync_promo.SyncPromo));
