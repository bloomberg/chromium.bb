// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ResetProfileSettingsOverlay class
   * Encapsulated handling of the 'Reset Profile Settings' overlay page.
   * @class
   */
  function ResetProfileSettingsOverlay() {
    OptionsPage.call(
        this, 'resetProfileSettings',
        loadTimeData.getString('resetProfileSettingsOverlayTabTitle'),
        'reset-profile-settings-overlay');
  }

  cr.addSingletonGetter(ResetProfileSettingsOverlay);

  ResetProfileSettingsOverlay.prototype = {
    // Inherit ResetProfileSettingsOverlay from OptionsPage.
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var f = this.updateCommitButtonState_.bind(this);
      var types = ['browser.reset_profile_settings.default_search_engine',
                   'browser.reset_profile_settings.homepage',
                   'browser.reset_profile_settings.content_settings',
                   'browser.reset_profile_settings.cookies_and_site_data',
                   'browser.reset_profile_settings.extensions'];
      types.forEach(function(type) {
          Preferences.getInstance().addEventListener(type, f);
      });

      var checkboxes = document.querySelectorAll(
          '#reset-profile-settings-content-area input[type=checkbox]');
      for (var i = 0; i < checkboxes.length; i++)
        checkboxes[i].onclick = f;
      this.updateCommitButtonState_();

      $('reset-profile-settings-dismiss').onclick = function(event) {
        ResetProfileSettingsOverlay.dismiss();
      };
      $('reset-profile-settings-commit').onclick = function(event) {
        ResetProfileSettingsOverlay.setResettingState(true);
        chrome.send('performResetProfileSettings');
      };
    },

    // Sets the enabled state of the commit button.
    updateCommitButtonState_: function() {
      var sel =
          '#reset-profile-settings-content-area input[type=checkbox]:checked';
      $('reset-profile-settings-commit').disabled =
        !document.querySelector(sel);
    },
  };

  /**
   * Enables/disables UI elements after/while Chrome is performing a reset.
   * @param {boolean} state If true, UI elements are disabled.
   */
  ResetProfileSettingsOverlay.setResettingState = function(state) {
    $('reset-default-search-engine-checkbox').disabled = state;
    $('reset-homepage-checkbox').disabled = state;
    $('reset-content-settings-checkbox').disabled = state;
    $('reset-cookies-and-site-data-checkbox').disabled = state;
    $('reset-extensions-checkbox').disabled = state;
    $('reset-extensions-handling').disabled = state;
    $('reset-profile-settings-throbber').style.visibility =
        state ? 'visible' : 'hidden';
    $('reset-profile-settings-dismiss').disabled = state;

    if (state)
      $('reset-profile-settings-commit').disabled = true;
    else
      ResetProfileSettingsOverlay.getInstance().updateCommitButtonState_();
  };

  /**
   * Chrome callback to notify ResetProfileSettingsOverlay that the reset
   * operation has terminated.
   */
  ResetProfileSettingsOverlay.doneResetting = function() {
    // The delay gives the user some feedback that the resetting
    // actually worked. Otherwise the dialog just vanishes instantly in most
    // cases.
    window.setTimeout(function() {
      ResetProfileSettingsOverlay.dismiss();
    }, 200);
  };

  /**
   * Dismisses the overlay.
   */
  ResetProfileSettingsOverlay.dismiss = function() {
    OptionsPage.closeOverlay();
    ResetProfileSettingsOverlay.setResettingState(false);
  };

  // Export
  return {
    ResetProfileSettingsOverlay: ResetProfileSettingsOverlay
  };
});
