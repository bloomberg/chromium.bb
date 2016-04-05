// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-reset-profile-dialog' is the dialog shown for clearing profile
 * settings.
 */
Polymer({
  is: 'settings-reset-profile-dialog',

  behaviors: [WebUIListenerBehavior],

  properties: {
    feedbackInfo_: String,
  },

  /** @private {!settings.ResetBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.ResetBrowserProxyImpl.getInstance();

    this.addEventListener('iron-overlay-canceled', function() {
      this.browserProxy_.onHideResetProfileDialog();
    }.bind(this));

    this.addWebUIListener('feedback-info-changed', function(feedbackInfo) {
      this.feedbackInfo_ = feedbackInfo;
    }.bind(this));
  },

  open: function() {
    this.$.dialog.open();
    this.browserProxy_.onShowResetProfileDialog();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onResetTap_: function() {
    this.$.resetSpinner.active = true;
    this.browserProxy_.performResetProfileSettings(
        this.$.sendSettings.checked).then(function() {
      this.$.resetSpinner.active = false;
      this.$.dialog.close();
      this.dispatchEvent(new CustomEvent('reset-done'));
    }.bind(this));
  },

  /** @private */
  onSendSettingsChange_: function() {
    // TODO(dpapad): Update how settings info is surfaced when final mocks
    // exist.
    this.$.settings.hidden = !this.$.sendSettings.checked;
    this.$.dialog.center();
  },
});
