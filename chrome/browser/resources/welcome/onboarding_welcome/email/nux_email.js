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

  /** @override */
  ready: function() {
    this.$.emailChooser.appProxy = nux.EmailAppProxyImpl.getInstance();
    this.$.emailChooser.metricsManager =
        new nux.ModuleMetricsManager(nux.EmailMetricsProxyImpl.getInstance());
  },

  onRouteEnter: function() {
    this.$.emailChooser.onRouteEnter();
  },

  onRouteExit: function() {
    this.$.emailChooser.onRouteExit();
  },

  onRouteUnload: function() {
    this.$.emailChooser.onRouteUnload();
  },
});
