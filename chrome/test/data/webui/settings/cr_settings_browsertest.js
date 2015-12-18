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
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'checkbox_tests.js',
    'dropdown_menu_tests.js',
    'pref_util_tests.js',
    'prefs_test_cases.js',
    'prefs_tests.js',
    'reset_page_test.js',
    'site_list_tests.js',
    'site_settings_category_tests.js',
  ]),
};

// Have to include command_line.h manually due to GEN calls below.
GEN('#include "base/command_line.h"');

// Times out on memory bots. http://crbug.com/534718
GEN('#if defined(MEMORY_SANITIZER)');
GEN('#define MAYBE_CrSettingsTest DISABLED_CrSettingsTest');
GEN('#else');
GEN('#define MAYBE_CrSettingsTest CrSettingsTest');
GEN('#endif');

// Runs all tests.
TEST_F('CrSettingsBrowserTest', 'MAYBE_CrSettingsTest', function() {
  // Register mocha tests for each element.
  settings_checkbox.registerTests();
  settings_dropdown_menu.registerTests();
  settings_prefUtil.registerTests();
  settings_prefs.registerTests();
  site_list.registerTests();
  site_settings_category.registerTests();

  // Run all registered tests.
  mocha.run();
});


TEST_F('CrSettingsBrowserTest', 'ResetPage', function() {
  settings_reset_page.registerDialogTests();
  settings_reset_page.registerBannerTests();
  mocha.run();
});
