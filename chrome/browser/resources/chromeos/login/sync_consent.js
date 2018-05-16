// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

Polymer({
  is: 'sync-consent',

  behaviors: [I18nBehavior],

  focus: function() {
    this.$.syncConsentOverviewDialog.focus();
  },

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue' button.
   * @private
   */
  onSettingsSaveAndContinue_: function() {
    if (this.$.reviewSettingsBox.checked) {
      chrome.send('login.SyncConsentScreen.userActed', ['continue-and-review']);
    } else {
      chrome.send(
          'login.SyncConsentScreen.userActed', ['continue-with-defaults']);
    }
  },
});
