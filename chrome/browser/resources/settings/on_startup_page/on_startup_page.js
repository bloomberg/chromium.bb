// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is a settings page.
 */
Polymer({
  is: 'settings-on-startup-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private {?NtpExtension} */
    ntpExtension_: Object,

    /**
     * Enum values for the 'session.restore_on_startup' preference.
     * @private {!Object<string, number>}
     */
    prefValues_: {
      readOnly: true,
      type: Object,
      value: {
        CONTINUE: 1,
        OPEN_NEW_TAB: 5,
        OPEN_SPECIFIC: 4,
      },
    },
  },

  /** @override */
  attached: function() {
    this.getNtpExtension_();
    this.addWebUIListener('update-ntp-extension', ntpExtension => {
      // Note that |ntpExtension| is empty if there is no NTP extension.
      this.ntpExtension_ = ntpExtension;
    });
  },

  /** @private */
  getNtpExtension_: function() {
    settings.OnStartupBrowserProxyImpl.getInstance().getNtpExtension().then(
        function(ntpExtension) {
          this.ntpExtension_ = ntpExtension;
        }.bind(this));
  },

  /**
   * @param {?NtpExtension} ntpExtension
   * @param {number} restoreOnStartup Value of prefs.session.restore_on_startup.
   * @return {boolean}
   * @private
   */
  showIndicator_: function(ntpExtension, restoreOnStartup) {
    return !!ntpExtension && restoreOnStartup == this.prefValues_.OPEN_NEW_TAB;
  },

  /**
   * Determine whether to show the user defined startup pages.
   * @param {number} restoreOnStartup Enum value from prefValues_.
   * @return {boolean} Whether the open specific pages is selected.
   * @private
   */
  showStartupUrls_: function(restoreOnStartup) {
    return restoreOnStartup == this.prefValues_.OPEN_SPECIFIC;
  },
});
