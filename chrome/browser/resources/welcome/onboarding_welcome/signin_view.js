// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-view',

  behaviors: [welcome.NavigationBehavior],

  /** @private */
  onSignInClick_: function() {
    // TODO(scottchen): create a feature flag to direct part of the users to
    //     chrome://welcome/email-interstitial instead of NTP.
    // TODO(scottchen): implement chrome://welcome/email-interstitial.
    welcome.WelcomeBrowserProxyImpl.getInstance().handleActivateSignIn();
  },

  /** @private */
  onNoThanksClick_: function() {
    welcome.navigateToNextStep();
  }
});
