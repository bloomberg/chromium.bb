// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'nux-google-apps',

  behaviors: [welcome.NavigationBehavior],

  properties: {
    /** @private */
    hasAppsSelected_: Boolean,

    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,
  },

  /** @private */
  finalized_: false,

  /** @override */
  ready: function() {
    window.addEventListener('beforeunload', () => {
      if (this.finalized_)
        return;
      // TODO(hcarmona): Add metrics.
      this.$.appChooser.removeAllBookmarks();
    });
  },

  onRouteEnter: function() {
    this.finalized_ = false;
    this.$.appChooser.populateAllBookmarks();
  },

  onRouteExit: function() {
    if (this.finalized_)
      return;
    // TODO(hcarmona): Add metrics?
    this.$.appChooser.removeAllBookmarks();
  },

  /** @private */
  onNoThanksClicked_: function() {
    // TODO(hcarmona): Add metrics.
    this.$.appChooser.removeAllBookmarks();
    welcome.navigateToNextStep();
  },

  /** @private */
  onGetStartedClicked_: function() {
    // TODO(hcarmona): Add metrics.
    this.finalized_ = true;
    welcome.navigateToNextStep();
  },
});
