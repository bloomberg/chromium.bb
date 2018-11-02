// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'nux-email',

  behaviors: [welcome.NavigationBehavior],

  properties: {
    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,
  },

  /**
   * This element can receive an |onRouteChange| notification after it's
   * detached. This will make it no-op.
   * @private
   */
  isDetached_: false,

  /** @override */
  detached: function() {
    this.isDetached_ = true;
  },

  /**
   * Elements can override onRouteChange to handle route changes.
   * Overrides function in behavior.
   * @param {!welcome.Routes} route
   * @param {number} step
   */
  onRouteChange: function(route, step) {
    if (`step-${step}` == this.id) {
      nux.BookmarkProxyImpl.getInstance().isBookmarkBarShown().then(
          bookmarkBarShown => {
            if (this.isDetached_)
              return;

            this.$.emailChooser.bookmarkBarWasShown = bookmarkBarShown;
            this.$.emailChooser.addBookmark();
          });
    } else {
      this.$.emailChooser.removeBookmark();
    }
  },
});
