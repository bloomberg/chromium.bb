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

  /**
   * Elements can override onRouteChange to handle route changes.
   * Overrides function in behavior.
   * @param {!welcome.Routes} route
   * @param {number} step
   */
  onRouteChange: function(route, step) {
    if (`step-${step}` == this.id)
      this.$.appChooser.populateAllBookmarks();
  },

  /** @private */
  onNoThanksClicked_: function() {
    chrome.send('rejectGoogleApps');
    welcome.navigateToNextStep();
  },

  /** @private */
  onGetStartedClicked_: function() {
    let selectedApps = this.$.appChooser.getSelectedAppList();
    nux.NuxGoogleAppsProxyImpl.getInstance().addGoogleApps(selectedApps);
    welcome.navigateToNextStep();
  },
});
