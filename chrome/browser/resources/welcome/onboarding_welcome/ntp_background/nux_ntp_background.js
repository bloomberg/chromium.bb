// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
Polymer({
  is: 'nux-ntp-background',

  behaviors: [welcome.NavigationBehavior],

  properties: {
    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,
  },

  /** @private {?Array<!nux.NtpBackgroundData>} */
  backgrounds_: null,

  /** @private {?nux.NtpBackgroundProxy} */
  ntpBackgroundProxy_: null,

  /** @override */
  ready: function() {
    this.ntpBackgroundProxy_ = nux.NtpBackgroundProxyImpl.getInstance();
  },

  onRouteEnter: function() {
    this.ntpBackgroundProxy_.getBackgrounds().then((backgrounds) => {
      this.backgrounds_ = backgrounds;
    });
  },

  /** @private */
  onNextClicked_: function() {
    welcome.navigateToNextStep();
  },

  /** @private */
  onSkipClicked_: function() {
    welcome.navigateToNextStep();
  },
});
