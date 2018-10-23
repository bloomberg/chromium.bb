// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'nux-google-apps',

  properties: {
    /** @private */
    hasAppsSelected_: Boolean,

    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,
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
