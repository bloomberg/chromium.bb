// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: the native-side handler for this is ResetProfileSettingsHandler.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * ResetProfileSettingsBanner class
   * Provides encapsulated handling of the Reset Profile Settings banner.
   * @constructor
   */
  function ResetProfileSettingsBanner() {}

  cr.addSingletonGetter(ResetProfileSettingsBanner);

  ResetProfileSettingsBanner.prototype = {
    /**
     * Whether or not the banner has already been dismissed.
     *
     * This is needed because of the surprising ordering of asynchronous
     * JS<->native calls when the settings page is opened with specifying a
     * given sub-page, e.g. chrome://settings/resetProfileSettings.
     *
     * In such a case, ResetProfileSettingsOverlay's didShowPage(), which calls
     * our dismiss() method, would be called before the native Handlers'
     * InitalizePage() methods have an effect in the JS, which includes calling
     * our show() method. This would mean that the banner would be first
     * dismissed, then shown. We want to prevent this.
     *
     * @type {boolean}
     * @private
     */
    hadBeenDismissed_: false,

    /**
     * Initializes the banner's event handlers.
     */
    initialize: function() {
      $('reset-profile-settings-banner-close').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticReset_WebUIBanner_ManuallyClosed']);
        ResetProfileSettingsBanner.dismiss();
      };
      $('reset-profile-settings-banner-activate').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticReset_WebUIBanner_ResetClicked']);
        OptionsPage.navigateToPage('resetProfileSettings');
      };
    },

    /**
     * Called by the native code to show the banner if needed.
     * @private
     */
    show_: function() {
      if (!this.hadBeenDismissed_) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticReset_WebUIBanner_BannerShown']);
        this.setVisibility_(true);
      }
    },

    /**
     * Called when the banner should be closed as a result of something taking
     * place on the WebUI page, i.e. when its close button is pressed, or when
     * the confirmation dialog for the profile settings reset feature is opened.
     * @private
     */
    dismiss_: function() {
      chrome.send('onDismissedResetProfileSettingsBanner');
      this.hadBeenDismissed_ = true;
      this.setVisibility_(false);
    },

    /**
     * Sets whether or not the reset profile settings banner shall be visible.
     * @param {boolean} show Whether or not to show the banner.
     * @private
     */
    setVisibility_: function(show) {
      $('reset-profile-settings-banner').hidden = !show;
    }
  };

  // Forward public APIs to private implementations.
  [
    'show',
    'dismiss',
  ].forEach(function(name) {
    ResetProfileSettingsBanner[name] = function() {
      var instance = ResetProfileSettingsBanner.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    ResetProfileSettingsBanner: ResetProfileSettingsBanner
  };
});
