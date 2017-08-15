// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Accessibility Settings tests. */

/** @const {string} Path to root from chrome/test/data/webui/settings/a11y. */
var ROOT_PATH = '../../../../../../';

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  'accessibility_audit_rules.js',
  'accessibility_test.js',
  ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js',
  ROOT_PATH + 'third_party/axe-core/axe.js',
]);

/**
 * Test fixture for Accessibility of Chrome Settings.
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsAccessibilityTest() {}

// Default accessibility audit options. Specify in test definition to use.
SettingsAccessibilityTest.axeOptions = {
  'rules': {
    // TODO(hcarmona): enable 'region' after addressing violation.
    'region': {enabled: false},
    // Disable 'skip-link' check since there are few tab stops before the main
    // content.
    'skip-link': {enabled: false},
  }
};

// Default accessibility audit options. Specify in test definition to use.
SettingsAccessibilityTest.violationFilter = {
  // TODO(quacht): remove this exception once the color contrast issue is
  // solved.
  // http://crbug.com/748608
  'color-contrast': function(nodeResult) {
    return nodeResult.element.id === 'prompt';
  },
  // Polymer components use aria-active-attribute.
  'aria-valid-attr': function(nodeResult) {
    return nodeResult.element.hasAttribute('aria-active-attribute');
  },
};

SettingsAccessibilityTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/',

  // Include files that define the mocha tests.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../ensure_lazy_loaded.js',
  ]),

  // TODO(hcarmona): Remove once ADT is not longer in the testing infrastructure
  runAccessibilityChecks: false,

  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    settings.ensureLazyLoaded();
  },
};