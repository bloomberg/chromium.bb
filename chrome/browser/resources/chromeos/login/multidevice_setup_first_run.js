// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview MultiDevice setup flow Polymer element to be used in the first
 *     run (i.e., after OOBE or during the user's first login on this
 *     Chromebook).
 */
Polymer({
  is: 'multidevice-setup-first-run',

  /** @private */
  onSkipSetupClicked_: function() {
    this.exitSetupFlow_();
  },

  /** @private */
  onAcceptClicked_: function() {
    this.exitSetupFlow_();
  },

  /** @private */
  exitSetupFlow_: function() {
    chrome.send('login.MultiDeviceSetupScreen.userActed', ['setup-finished']);
  }
});
