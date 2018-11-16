// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'signin-view',

  behaviors: [welcome.NavigationBehavior],

  /** @private {boolean} */
  shouldShowEmailInterstitial_:
      loadTimeData.getBoolean('showEmailInterstitial'),

  /** @private {?welcome.WelcomeBrowserProxy} */
  welcomeBrowserProxy_: null,

  /** @override */
  ready: function() {
    this.welcomeBrowserProxy_ = welcome.WelcomeBrowserProxyImpl.getInstance();
  },

  /**
   * @return {?string}
   * @private
   */
  getTargetUrl_: function() {
    const savedProvider =
        nux.NuxEmailProxyImpl.getInstance().getSavedProvider();
    if (savedProvider != undefined && this.shouldShowEmailInterstitial_) {
      return `chrome://welcome/email-interstitial?provider=${savedProvider}`;
    } else {
      return null;
    }
  },

  /**
   * When the user clicks sign-in, check whether or not they previously
   * selected an email provider they prefer to use. If so, direct them back to
   * the email-interstitial page, otherwise let it direct to NTP.
   * @private
   */
  onSignInClick_: function() {
    this.welcomeBrowserProxy_.handleActivateSignIn(this.getTargetUrl_());
  },

  /** @private */
  onNoThanksClick_: function() {
    // It's safe to assume sign-view is always going to be the last step, so
    // either go to the target url directly, or go to NTP directly.
    const targetUrl = this.getTargetUrl_();
    if (targetUrl) {
      this.welcomeBrowserProxy_.goToURL(targetUrl);
    } else {
      this.welcomeBrowserProxy_.goToNewTabPage();
    }
  }
});
