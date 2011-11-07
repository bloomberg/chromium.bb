// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(sail): Refactor options_page and remove this include.
<include src="options/options_page.js"/>
<include src="sync_setup_overlay.js"/>

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

  // Replicating enum from chrome/common/extensions/extension_constants.h.
  const actions = (function() {
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
      $('sync-setup-login-promo-column').hidden = false;
      $('promo-skip').hidden = false;

      this.showSetupUI_();
      chrome.send('SyncPromo:Initialize');

      var self = this;

      $('promo-skip-button').addEventListener('click', function() {
        chrome.send('SyncPromo:UserFlowAction', [actions.SKIP_CLICKED]);
        self.closeOverlay_();
      });

      var learnMoreClickedAlready = false;
      $('promo-learn-more-show').addEventListener('click', function() {
        self.showLearnMore_(true);
        if (!learnMoreClickedAlready)
          chrome.send('SyncPromo:UserFlowAction', [actions.LEARN_MORE_CLICKED]);
        learnMoreClickedAlready = true;
      });

      $('promo-learn-more-hide').addEventListener('click', function() {
        self.showLearnMore_(false);
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
        if (!signInAttemptedAlready)
          chrome.send('SyncPromo:UserFlowAction', [actions.SIGN_IN_ATTEMPTED]);
        signInAttemptedAlready = true;
      });

      var encryptionHelpClickedAlready = false;
      $('encryption-help-link').addEventListener('click', function() {
        if (!encryptionHelpClickedAlready )
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

      this.infographic_ = $('promo-infographic');
      this.learnMore_ = $('promo-information');

      this.infographic_.addEventListener('webkitTransitionEnd',
                                         this.toggleHidden_.bind(this));
      this.learnMore_.addEventListener('webkitTransitionEnd',
                                       this.toggleHidden_.bind(this));
    },

    /**
     * Remove the [hidden] attribute from the node that was not previously
     * transitioning.
     * @param {Event} e A -webkit-transition end event.
     * @private
     */
    toggleHidden_: function(e) {
      // Only show the other element if the target of this event was hidden
      // (showing also triggers a transition end).
      if (e.target.hidden) {
        if (e.target === this.infographic_)
          this.learnMore_.hidden = false;
        else
          this.infographic_.hidden = false;
      }
    },

    /**
     * Shows or hides the sync information.
     * @param {Boolean} show True if sync information should be shown, false
     *     otherwise.
     * @private
     */
    showLearnMore_: function(show) {
      $('promo-learn-more-show').hidden = show;
      $('promo-learn-more-hide').hidden = !show;
      // Setting [hidden] triggers a transition, which (when ended) will trigger
      // this.toggleHidden_.
      (show ? this.infographic_ : this.learnMore_).hidden = true;
    },

    /** @inheritDoc */
    sendConfiguration_: function() {
      chrome.send('SyncPromo:UserFlowAction',
                  [actions.CONFIRMED_AFTER_SIGN_IN]);
      options.SyncSetupOverlay.prototype.sendConfiguration_.apply(this,
          arguments);
    },

    /**
     * Shows or hides the title of the promo page.
     * @param {Boolean} visible true if the title should be visible, false
     *     otherwise.
     * @private
     */
    setPromoTitleVisible_: function(visible) {
      $('promo-title').hidden = !visible;
    }
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

  SyncPromo.setPromoTitleVisible = function(visible) {
    SyncPromo.getInstance().setPromoTitleVisible_(visible);
  }

  // Export
  return {
    SyncPromo : SyncPromo
  };
});

var OptionsPage = options.OptionsPage;
var SyncSetupOverlay = sync_promo.SyncPromo;
window.addEventListener('DOMContentLoaded', sync_promo.SyncPromo.initialize);
