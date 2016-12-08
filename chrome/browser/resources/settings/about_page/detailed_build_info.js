// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-detailed-build-info' contains detailed build
 * information for ChromeOS.
 */

Polymer({
  is: 'settings-detailed-build-info',

  behaviors: [I18nBehavior],

  properties: {
    /** @private {!VersionInfo} */
    versionInfo_: Object,

    /** @private */
    currentlyOnChannelText_: String,

    /** @private */
    showChannelSwitcherDialog_: Boolean,

    /** @private */
    canChangeChannel_: Boolean,
  },

  /** @override */
  ready: function() {
    var browserProxy = settings.AboutPageBrowserProxyImpl.getInstance();
    browserProxy.pageReady();

    browserProxy.getVersionInfo().then(function(versionInfo) {
      this.versionInfo_ = versionInfo;
    }.bind(this));

    this.updateChannelInfo_();
  },

  /** @private */
  updateChannelInfo_: function() {
    var browserProxy = settings.AboutPageBrowserProxyImpl.getInstance();
    browserProxy.getChannelInfo().then(function(info) {
      // Display the target channel for the 'Currently on' message.
      this.currentlyOnChannelText_ = this.i18n(
          'aboutCurrentlyOnChannel',
          this.i18n(settings.browserChannelToI18nId(info.targetChannel)));
      this.canChangeChannel_ = info.canChangeChannel;
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

  /**
   * @param {!Event} e
   * @private
   */
  onChangeChannelTap_: function(e) {
    e.preventDefault();
    this.showChannelSwitcherDialog_ = true;
    // Async to wait for dialog to appear in the DOM.
    this.async(function() {
      var dialog = this.$$('settings-channel-switcher-dialog');
      // Register listener to detect when the dialog is closed. Flip the boolean
      // once closed to force a restamp next time it is shown such that the
      // previous dialog's contents are cleared.
      dialog.addEventListener('close', function() {
        this.showChannelSwitcherDialog_ = false;
        this.updateChannelInfo_();
      }.bind(this));
    }.bind(this));
  },
});
