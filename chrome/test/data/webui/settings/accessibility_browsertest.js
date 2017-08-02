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
  ROOT_PATH + 'third_party/axe-core/axe.js',
]);

/**
 * @typedef {{
 *   rules: {
 *     'color_contrast': ({ enabled: boolean} | undefined),
 *     'aria_valid_attr': ({ enabled: boolean} | undefined),
 *   }
 * }}
 * @see https://github.com/dequelabs/axe-core/blob/develop/doc/API.md#options-parameter
 */
function AccessibilityAuditConfig() {}

/**
 * Test fixture for Accessibility of Chrome Settings.
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsAccessibilityTest() {}

/**
 * Run aXe-core accessibility audit, print console-friendly representation
 * of violations to console, and fail the test if there are audit failures.
 * @param {AccessibilityAuditConfig} options Dictionary disabling specific
 *    audit rules.
 * @return {Promise} A promise resolved if the accessibility audit completes
 *    with no issues, or rejected if the audit finds any accessibility issues.
 */
SettingsAccessibilityTest.runAudit = function(options) {
  // Ignore iron-iconset-svg elements that have duplicate ids and result in
  // false postives from the audit.
  var context = {exclude: ['iron-iconset-svg']};
  options = options || {};

  return new Promise(function(resolve, reject) {
    axe.run(context, options, function(err, results) {
      if (err)
        reject(err);

      var violationCount = results.violations.length;
      if (violationCount) {
        // Pretty print out the violations detected by the audit.
        console.log(JSON.stringify(results.violations, null, 4));
        reject('Found ' + violationCount + ' accessibility violations.');
      } else {
        resolve();
      }
    });
  });
};

SettingsAccessibilityTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/',

  // Include files that define the mocha tests.
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'ensure_lazy_loaded.js', 'passwords_and_autofill_fake_data.js',
    'passwords_a11y_test.js'
  ]),

  // TODO(hcarmona): Remove once ADT is not longer in the testing infrastructure
  runAccessibilityChecks: false,

  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    settings.ensureLazyLoaded();
  },
};

// TODO(quacht): Enable in separate CL.
// Test disabled since it doesn't work on all platforms.
// TEST_F('SettingsAccessibilityTest', 'All', function() {
//   mocha.run();
// });

GEN('#endif  // defined(NDEBUG)');