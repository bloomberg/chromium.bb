// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-on-startup-page' is a settings page.
 */
Polymer({
  is: 'settings-on-startup-page',

  properties: {
    prefs: Object,
  },

  /** @private */
  onManageStartupUrls_: function() {
    settings.navigateTo(settings.Route.STARTUP_URLS);
  },
});
