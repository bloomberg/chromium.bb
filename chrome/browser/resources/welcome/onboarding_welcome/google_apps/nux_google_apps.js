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

  /**
   * Elements can override onRouteChange to handle route changes.
   * Overrides function in behavior.
   * @param {!welcome.Routes} route
   * @param {number} step
   */
  onRouteChange: function(route, step) {
    if (`step-${step}` == this.id) {
      this.finalized_ = false;
      nux.BookmarkProxyImpl.getInstance().isBookmarkBarShown().then(
          bookmarkBarShown => {
            this.$.appChooser.bookmarkBarWasShown = bookmarkBarShown;
          });
      this.$.appChooser.populateAllBookmarks();
    } else {
      if (this.finalized_)
        return;
      // TODO(hcarmona): Add metrics?
      this.$.appChooser.removeAllBookmarks();
    }
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
