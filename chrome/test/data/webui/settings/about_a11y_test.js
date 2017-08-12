// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Define accessibility tests for the ABOUT route.
 */

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_browsertest.js',
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
  tests: {
    'Accessible with No Changes': function() {}
  },
  /** @override */
  violationFilter: {
    // TODO(quacht): remove this exception once the color contrast issue is
    // solved.
    // http://crbug.com/748608
    'color-contrast': function(nodeResult) {
      return nodeResult.element.id === 'prompt';
    },
    'aria-valid-attr': function(nodeResult) {
      return nodeResult.element.hasAttribute('aria-active-attribute');
    },
  }
});