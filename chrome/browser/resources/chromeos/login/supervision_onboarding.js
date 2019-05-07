// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Supervision Onboarding polymer element. It forwards user input
 * to the C++ handler.
 */
Polymer({
  is: 'supervision-onboarding',

  behaviors: [LoginScreenBehavior],

  /** @override */
  ready: function() {
    this.initializeLoginScreen('SupervisionOnboardingScreen', {
      commonScreenSize: true,
      resetAllowed: true,
    });
  },

  /** @private */
  onBack_: function() {
    this.exitSetupFlow_();
  },

  /** @private */
  onSkip_: function() {
    this.exitSetupFlow_();
  },

  /** @private */
  onNext_: function() {
    this.exitSetupFlow_();
  },

  /** @private */
  exitSetupFlow_: function() {
    chrome.send('login.SupervisionOnboardingScreen.userActed',
        ['setup-finished']);
  }
});
