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
      this.showMetricName = 'AutomaticSettingsReset_WebUIBanner_BannerShown';

      this.dismissNativeCallbackName =
          'onDismissedAutomaticSettingsResetBanner';

      this.visibilityDomElement = $('automatic-settings-reset-banner');

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
  };

  // Forward public APIs to protected implementations.
  [
    'show',
    'dismiss',
  ].forEach(function(name) {
    AutomaticSettingsResetBanner[name] = function() {
      var instance = AutomaticSettingsResetBanner.getInstance();
      return instance[name].apply(instance, arguments);
    };
  });

  // Export
  return {
    AutomaticSettingsResetBanner: AutomaticSettingsResetBanner
  };
});
