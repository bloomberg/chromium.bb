// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'nux-google-apps',

  behaviors: [welcome.NavigationBehavior],

  properties: {
    /** @type {nux.stepIndicatorModel} */
    indicatorModel: Object,
  },

  onRouteEnter: function() {
    this.$.appChooser.onRouteEnter();
  },

  onRouteExit: function() {
    this.$.appChooser.onRouteExit();
  },

  onRouteUnload: function() {
    this.$.appChooser.onRouteUnload();
  },
});
