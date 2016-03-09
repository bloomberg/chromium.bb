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

  properties: {
    feedbackInfo_: String,
  },

  /** @override */
  attached: function() {
    cr.define('SettingsResetPage', function() {
      return {
        doneResetting: function() {
          this.$.resetSpinner.active = false;
          this.$.dialog.close();
          this.dispatchResetDoneEvent();
        }.bind(this),

        setFeedbackInfo: function(data) {
          this.set('feedbackInfo_', data.feedbackInfo);
        }.bind(this),
      };
    }.bind(this));
  },

  /** @override */
  ready: function() {
    this.addEventListener('iron-overlay-canceled', function() {
      chrome.send('onHideResetProfileDialog');
    });
  },

  dispatchResetDoneEvent: function() {
    this.dispatchEvent(new CustomEvent('reset-done'));
  },

  open: function() {
    this.$.dialog.open();
    chrome.send('onShowResetProfileDialog');
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onResetTap_: function() {
    this.$.resetSpinner.active = true;
    chrome.send('performResetProfileSettings', [this.$.sendSettings.checked]);
  },

  /** @private */
  onSendSettingsChange_: function() {
    // TODO(dpapad): Update how settings info is surfaced when final mocks
    // exist.
    this.$.settings.hidden = !this.$.sendSettings.checked;
    this.$.dialog.center();
  },
});
