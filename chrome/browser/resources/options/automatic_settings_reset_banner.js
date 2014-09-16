// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: the native-side handler for this is AutomaticSettingsResetHandler.

cr.define('options', function() {
  /** @const */ var SettingsBannerBase = options.SettingsBannerBase;
  /** @const */ var PageManager = cr.ui.pageManager.PageManager;

  /**
   * AutomaticSettingsResetBanner class
   * Provides encapsulated handling of the Reset Profile Settings banner.
   * @constructor
   * @extends {options.SettingsBannerBase}
   */
  function AutomaticSettingsResetBanner() {}

  cr.addSingletonGetter(AutomaticSettingsResetBanner);

  AutomaticSettingsResetBanner.prototype = {
    __proto__: SettingsBannerBase.prototype,

    /**
     * Initializes the banner's event handlers.
     */
    initialize: function() {
      this.showMetricName_ = 'AutomaticSettingsReset_WebUIBanner_BannerShown';

      this.dismissNativeCallbackName_ =
          'onDismissedAutomaticSettingsResetBanner';

      this.setVisibilibyDomElement_ = $('automatic-settings-reset-banner');

      $('automatic-settings-reset-banner-close').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_ManuallyClosed']);
        AutomaticSettingsResetBanner.dismiss();
      };
      $('automatic-settings-reset-learn-more').onclick = function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_LearnMoreClicked']);
      };
      $('automatic-settings-reset-banner-activate-reset').onclick =
          function(event) {
        chrome.send('metricsHandler:recordAction',
            ['AutomaticSettingsReset_WebUIBanner_ResetClicked']);
        PageManager.showPageByName('resetProfileSettings');
      };
    },

    /**
     * Called by the native code to show the banner if needed.
     * @private
     */
    show_: function() {
      if (!this.hadBeenDismissed_) {
        chrome.send('metricsHandler:recordAction', [this.showMetricName_]);
        this.setVisibility(true);
      }
    },

    /**
     * Called when the banner should be closed as a result of something taking
     * place on the WebUI page, i.e. when its close button is pressed, or when
     * the confirmation dialog for the profile settings reset feature is opened.
     * @private
     */
    dismiss_: function() {
      chrome.send(this.dismissNativeCallbackName_);
      this.hadBeenDismissed_ = true;
      this.setVisibility(false);
    },
  };

  // Forward public APIs to private implementations.
  cr.makePublic(AutomaticSettingsResetBanner, [
    'show',
    'dismiss',
  ]);

  // Export
  return {
    AutomaticSettingsResetBanner: AutomaticSettingsResetBanner
  };
});
