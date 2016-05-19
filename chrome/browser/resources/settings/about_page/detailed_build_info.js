// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-detailed-build-info' contains detailed build
 * information for ChromeOS.
 */

(function() {

/**
 * @param {!BrowserChannel} channel
 * @return {string}
 */
function browserChannelToI18nId(channel) {
  switch (channel) {
    case BrowserChannel.BETA: return 'aboutChannelBeta';
    case BrowserChannel.DEV: return 'aboutChannelDev';
    case BrowserChannel.STABLE: return 'aboutChannelStable';
  }

  assertNotReached();
}

Polymer({
  is: 'settings-detailed-build-info',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!VersionInfo} */
    versionInfo_: Object,

    /** @private */
    currentlyOnChannelText_: String,
  },

  /** @override */
  ready: function() {
    var browserProxy = settings.AboutPageBrowserProxyImpl.getInstance();
    browserProxy.pageReady();

    browserProxy.getVersionInfo().then(function(versionInfo) {
      this.versionInfo_ = versionInfo;
    }.bind(this));
    browserProxy.getCurrentChannel().then(function(channel) {
      this.currentlyOnChannelText_ = this.i18n(
          'aboutCurrentlyOnChannel',
          this.i18n(browserChannelToI18nId(channel)));
    }.bind(this));
  },

  /**
   * @param {string} version
   * @return {boolean}
   * @private
   */
  shouldShowVersion_: function(version) {
    return version.length > 0;
  },
});

})();
