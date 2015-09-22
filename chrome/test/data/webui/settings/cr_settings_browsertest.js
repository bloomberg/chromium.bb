// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Settings tests. */

/** @const {string} Path to source root. */
var ROOT_PATH = '../../../../../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for Polymer Settings elements.
 * @constructor
 * @extends {PolymerTest}
*/
function CrSettingsBrowserTest() {}

CrSettingsBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/prefs/prefs.html',

  commandLineSwitches: [{switchName: 'enable-md-settings'}],

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'checkbox_tests.js',
    'prefs_test_cases.js',
    'prefs_tests.js',
  ]),
};

// Runs all tests.
TEST_F('CrSettingsBrowserTest', 'DISABLED_CrSettingsTest', function() {
  // Register mocha tests for each element.
  cr_settings_checkbox.registerTests();
  cr_settings_prefs.registerTests();

  // Run all registered tests.
  mocha.run();
});
