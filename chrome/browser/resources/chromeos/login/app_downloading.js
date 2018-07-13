// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design App Downloading
 * screen.
 */

Polymer({
  is: 'app-downloading',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  focus: function() {
    this.$['app-downloading-dialog'].focus();
  },

  /** @private */
  onContinue_: function() {
    chrome.send(
        'login.AppDownloadingScreen.userActed',
        ['appDownloadingContinueSetup']);
  },
});
