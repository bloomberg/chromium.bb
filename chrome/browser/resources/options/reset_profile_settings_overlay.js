// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;

  var AutomaticSettingsResetBanner = options.AutomaticSettingsResetBanner;

  /**
   * ResetProfileSettingsOverlay class
   *
   * Encapsulated handling of the 'Reset Profile Settings' and the 'Triggered
   * Reset Profile Settings' overlay pages. See triggered_profile_resetter.h for
   * when the triggered variant will be used.
   *
   * @constructor
   * @param {boolean} isTriggered Whether the overlay is the triggered variant.
   * @extends {cr.ui.pageManager.Page}
   */
  function ResetProfileSettingsOverlay(isTriggered) {
    this.isTriggered_ = isTriggered;
    Page.call(
        this,
        isTriggered ? 'triggeredResetProfileSettings' : 'resetProfileSettings',
        loadTimeData.getString(isTriggered ?
            'triggeredResetProfileSettingsOverlay' :
            'resetProfileSettingsOverlayTabTitle'),
        'reset-profile-settings-overlay');
  }

  ResetProfileSettingsOverlay.prototype = {
    // Inherit ResetProfileSettingsOverlay from Page.
    __proto__: Page.prototype,

    /**
     * Indicates whether the overlay is a triggered reset overlay.
     * @type {boolean}
     * @private
     */
    isTriggered_: false,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

      // Set the onclick handlers only once when initializing the regular reset
      // profile settings overlay.
      if (!this.isTriggered_) {
        $('reset-profile-settings-dismiss').onclick = function(e) {
          ResetProfileSettingsOverlay.dismiss();
        };
        $('reset-profile-settings-commit').onclick = function(e) {
          ResetProfileSettingsOverlay.setResettingState(true);
          chrome.send('performResetProfileSettings',
                      [$('send-settings').checked]);
        };
        $('expand-feedback').onclick = function(e) {
          var feedbackTemplate = $('feedback-template');
          feedbackTemplate.hidden = !feedbackTemplate.hidden;
          e.preventDefault();
        };
      }
    },

    /**
     * @override
     * @suppress {checkTypes}
     * TODO(vitalyp): remove the suppression. See the explanation in
     * chrome/browser/resources/options/automatic_settings_reset_banner.js.
     */
    didShowPage: function() {
      $('reset-profile-settings-title').textContent =
          loadTimeData.getString(this.isTriggered_ ?
              'triggeredResetProfileSettingsOverlay' :
              'resetProfileSettingsOverlay');
      $('reset-profile-settings-explanation').textContent =
          loadTimeData.getString(this.isTriggered_ ?
              'triggeredResetProfileSettingsExplanation' :
              'resetProfileSettingsExplanation');

      chrome.send('onShowResetProfileDialog');
    },

    /** @override */
    didClosePage: function() {
      chrome.send('onHideResetProfileDialog');
    },
  };

  /**
   * Enables/disables UI elements after/while Chrome is performing a reset.
   * @param {boolean} state If true, UI elements are disabled.
   */
  ResetProfileSettingsOverlay.setResettingState = function(state) {
    $('reset-profile-settings-throbber').style.visibility =
        state ? 'visible' : 'hidden';
    $('reset-profile-settings-dismiss').disabled = state;
    $('reset-profile-settings-commit').disabled = state;
  };

  /**
   * Chrome callback to notify ResetProfileSettingsOverlay that the reset
   * operation has terminated.
   * @suppress {checkTypes}
   * TODO(vitalyp): remove the suppression. See the explanation in
   * chrome/browser/resources/options/automatic_settings_reset_banner.js.
   */
  ResetProfileSettingsOverlay.doneResetting = function() {
    AutomaticSettingsResetBanner.dismiss();
    ResetProfileSettingsOverlay.dismiss();
  };

  /**
   * Dismisses the overlay.
   */
  ResetProfileSettingsOverlay.dismiss = function() {
    PageManager.closeOverlay();
    ResetProfileSettingsOverlay.setResettingState(false);
  };

  ResetProfileSettingsOverlay.setFeedbackInfo = function(feedbackListData) {
    var input = new JsEvalContext(feedbackListData);
    var output = $('feedback-template');
    jstProcess(input, output);
  };

  // Export
  return {
    ResetProfileSettingsOverlay: ResetProfileSettingsOverlay
  };
});
