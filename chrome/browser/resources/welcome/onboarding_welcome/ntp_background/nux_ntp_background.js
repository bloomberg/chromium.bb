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

  /** @private */
  onNextClicked_: function() {
    welcome.navigateToNextStep();
  },

  /** @private */
  onSkipClicked_: function() {
    welcome.navigateToNextStep();
  },
});
