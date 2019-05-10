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

  /** Overridden from LoginScreenBehavior. */
  EXTERNAL_API: [
    'setupMojo',
  ],

  /** @private {?chromeos.supervision.mojom.OnboardingControllerProxy} */
  controller_: null,

  /**
   * @private {?chromeos.supervision.mojom.
   *              OnboardingWebviewHostCallbackRouter}
   */
  hostCallbackRouter_: null,

  setupMojo: function() {
    this.controller_ =
            chromeos.supervision.mojom.OnboardingController.getProxy();

    this.hostCallbackRouter_ =
        new chromeos.supervision.mojom.OnboardingWebviewHostCallbackRouter();

    this.hostCallbackRouter_.loadPage.addListener(pageUrl => {
      this.$.contentWebview.src = pageUrl;
    });
    this.hostCallbackRouter_.exitFlow.addListener(() => {
      this.exitFlow_();
    });

    this.controller_.bindWebviewHost(this.hostCallbackRouter_.createProxy());
  },

  /** @override */
  ready: function() {
    this.initializeLoginScreen('SupervisionOnboardingScreen', {
      commonScreenSize: true,
      resetAllowed: true,
    });
  },

  /** @private */
  onBack_: function() {
    this.controller_.handleAction(
        chromeos.supervision.mojom.OnboardingFlowAction.kShowPreviousPage);
  },

  /** @private */
  onSkip_: function() {
    this.controller_.handleAction(
        chromeos.supervision.mojom.OnboardingFlowAction.kSkipFlow);
  },

  /** @private */
  onNext_: function() {
    this.controller_.handleAction(
        chromeos.supervision.mojom.OnboardingFlowAction.kShowNextPage);
  },

  /** @private */
  exitFlow_: function() {
    chrome.send('login.SupervisionOnboardingScreen.userActed',
        ['setup-finished']);
  }
});
