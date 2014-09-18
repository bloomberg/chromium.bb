// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;

  var AutomaticSettingsResetBanner = options.AutomaticSettingsResetBanner;
  var ResetProfileSettingsBanner = options.ResetProfileSettingsBanner;

  /**
   * ResetProfileSettingsOverlay class
   * Encapsulated handling of the 'Reset Profile Settings' overlay page.
   * @class
   */
  function ResetProfileSettingsOverlay() {
    Page.call(this, 'resetProfileSettings',
              loadTimeData.getString('resetProfileSettingsOverlayTabTitle'),
              'reset-profile-settings-overlay');
  }

  cr.addSingletonGetter(ResetProfileSettingsOverlay);

  ResetProfileSettingsOverlay.prototype = {
    // Inherit ResetProfileSettingsOverlay from Page.
    __proto__: Page.prototype,

    /** @override */
    initializePage: function() {
      Page.prototype.initializePage.call(this);

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
    },

    /** @override */
    didShowPage: function() {
      ResetProfileSettingsBanner.dismiss();
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
