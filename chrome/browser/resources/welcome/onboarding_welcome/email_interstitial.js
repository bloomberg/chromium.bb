// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview When user gets to chrome://welcome/email-interstitial, it will
 * also have a ?provider=... url param. This value is an ID that should match
 * one of the provider in the email list retrieved from the backend. If the user
 * chooses the continue button, they should be directed to the landing page of
 * that email provider.
 */
Polymer({
  is: 'email-interstitial',

  /** @private */
  welcomeBrowserProxy_: null,

  /** @private {?nux.EmailInterstitialProxy} */
  emailInterstitialProxy_: null,

  /** @private {boolean} */
  finalized_: false,

  /** @override */
  ready: function() {
    this.welcomeBrowserProxy_ = welcome.WelcomeBrowserProxyImpl.getInstance();
    this.emailInterstitialProxy_ = nux.EmailInterstitialProxyImpl.getInstance();
    this.emailInterstitialProxy_.recordPageShown();

    window.addEventListener('beforeunload', () => {
      // Both buttons on this page will navigate to a new URL.
      if (this.finalized_) {
        return;
      }
      this.finalized_ = true;
      this.emailInterstitialProxy_.recordNavigatedAway();
    });
  },

  /** @override */
  attached: function() {
    // Focus heading for accessibility.
    this.$$('h1').focus();
  },

  /** @private */
  onContinueClick_: function() {
    this.finalized_ = true;
    this.emailInterstitialProxy_.recordNext();
    const providerId =
        (new URL(window.location.href)).searchParams.get('provider');
    nux.EmailAppProxyImpl.getInstance().getAppList().then(list => {
      for (let i = 0; i < list.length; i++) {
        if (list[i].id == providerId) {
          this.welcomeBrowserProxy_.goToURL(list[i].url);
          return;
        }
      }

      // It shouldn't be possible to go to a URL with a non-existent provider
      // id.
      assertNotReached();
    });
  },

  /** @private */
  onNoThanksClick_: function() {
    this.finalized_ = true;
    this.emailInterstitialProxy_.recordSkip();
    this.welcomeBrowserProxy_.goToNewTabPage();
  }
});
