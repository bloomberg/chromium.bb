// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Accessibility Settings tests. */

// Disable in debug and memory sanitizer modes because of timeouts.
GEN('#if defined(NDEBUG)');

/** @const {string} Path to root from chrome/test/data/webui/settings/. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture and aXe-core accessibility audit.
GEN_INCLUDE([
  ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js',
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_audit_rules.js',
  ROOT_PATH + 'chrome/test/data/webui/settings/accessibility_audit.js',
  ROOT_PATH + 'third_party/axe-core/axe.js',
]);

/**
 * Test fixture for Accessibility of Chrome Settings.
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsAccessibilityTest() {}

SettingsAccessibilityTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/',

  // Include files that define the mocha tests.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'ensure_lazy_loaded.js',
    'passwords_and_autofill_fake_data.js',
    'passwords_a11y_test.js',
  ]),

  // TODO(hcarmona): Remove once ADT is not longer in the testing infrastructure
  runAccessibilityChecks: false,

  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    settings.ensureLazyLoaded();
  },
};

// TODO(quacht): Enable the audit rules failed in a separate CL after
// resolving the violation or after adding the violation exception framework.
// http://crbug.com/748608
// http://crbug.com/748632
var rulesToSkip = ['aria-valid-attr', 'color-contrast', 'region', 'skip-link'];
// Disable rules flaky for CFI build.
var flakyRules = ['meta-viewpoint', 'list', 'frame-title', 'label',
    'hidden_content', 'aria-valid-attr-value', 'button-name'];
rulesToSkip.concat(flakyRules);

// Define a unit test for every audit rule.
AccessibilityTest.ruleIds.forEach((ruleId) => {
  if (rulesToSkip.indexOf(ruleId) == -1) {
    // Replace hyphens, which break the build.
    var ruleName = ruleId.replace(new RegExp('-', 'g'), '_');
    TEST_F('SettingsAccessibilityTest', 'MANAGE_PASSWORDS_' + ruleName,
        () => {
          mocha.grep('MANAGE_PASSWORDS_' + ruleId).run();
        });
  }
});

GEN('#endif  // defined(NDEBUG)');