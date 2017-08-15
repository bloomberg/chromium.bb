// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the ABOUT route.
 */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

AccessibilityTest.define('SettingsAccessibilityTest', {
  /** @override */
  name: 'ABOUT',
  /** @override */
  axeOptions: {
    'rules': {
      // TODO(hcarmona): enable 'region' after addressing violation.
      'region': {enabled: false},
    }
  },
  /** @override */
  setup: function() {
    settings.router.navigateTo(settings.routes.ABOUT);
    Polymer.dom.flush();
  },
  /** @override */
  tests: {'Accessible with No Changes': function() {}},
  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});