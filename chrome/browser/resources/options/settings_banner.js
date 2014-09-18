// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(engedy): AutomaticSettingsResetBanner is the sole class to derive from
// SettingsBannerBase. Refactor this into automatic_settings_reset_banner.js.

cr.define('options', function() {

  /**
   * Base class for banners that appear at the top of the settings page.
   * @constructor
   */
  function SettingsBannerBase() {}

  cr.addSingletonGetter(SettingsBannerBase);

  SettingsBannerBase.prototype = {
    /**
     * Whether or not the banner has already been dismissed.
     *
     * This is needed because of the surprising ordering of asynchronous
     * JS<->native calls when the settings page is opened with specifying a
     * given sub-page, e.g. chrome://settings/AutomaticSettingsReset.
     *
     * In such a case, AutomaticSettingsResetOverlay's didShowPage(), which
     * calls our dismiss() method, would be called before the native Handlers'
     * InitalizePage() methods have an effect in the JS, which includes calling
     * our show() method. This would mean that the banner would be first
     * dismissed, then shown. We want to prevent this.
     *
     * @type {boolean}
     * @private
     */
    hadBeenDismissed_: false,

    /**
     * Metric name to send when a show event occurs.
     */
    showMetricName_: '',

    /**
     * Name of the native callback invoked when the banner is dismised.
     */
    dismissNativeCallbackName_: '',

    /**
     * DOM element whose visibility is set when setVisibility_ is called.
     */
    setVisibilibyDomElement_: null,

    /**
     * Sets whether or not the reset profile settings banner shall be visible.
     * @param {boolean} show Whether or not to show the banner.
     * @protected
     */
    setVisibility: function(show) {
      this.setVisibilibyDomElement_.hidden = !show;
    },
  };

  // Export
  return {
    SettingsBannerBase: SettingsBannerBase
  };
});
