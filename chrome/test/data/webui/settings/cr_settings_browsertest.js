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

  /**
   * TODO(dbeam): these should not be required monolithically.
   * @override
   */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'checkbox_tests.js',
    'dropdown_menu_tests.js',
    'pref_util_tests.js',
    'prefs_test_cases.js',
    'prefs_tests.js',
    'reset_page_test.js',
    'site_details_tests.js',
    'site_details_permission_tests.js',
    'site_list_tests.js',
    'site_settings_category_tests.js',
  ]),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');
  },
};

// Have to include command_line.h manually due to GEN calls below.
GEN('#include "base/command_line.h"');

// Times out on memory bots. http://crbug.com/534718
GEN('#if defined(MEMORY_SANITIZER)');
GEN('#define MAYBE_CrSettingsTest DISABLED_CrSettingsTest');
GEN('#else');
GEN('#define MAYBE_CrSettingsTest CrSettingsTest');
GEN('#endif');

// TODO(dbeam): these should be split into multiple TEST_F()s.
TEST_F('CrSettingsBrowserTest', 'MAYBE_CrSettingsTest', function() {
  settings_checkbox.registerTests();
  settings_dropdown_menu.registerTests();
  settings_prefUtil.registerTests();
  settings_prefs.registerTests();
  site_details.registerTests();
  site_details_permission.registerTests();
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

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsRtlTest() {}

CrSettingsRtlTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/settings_ui/settings_ui.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'rtl_tests.js',
  ]),
};

TEST_F('CrSettingsRtlTest', 'DrawerPanelFlips', function() {
  settingsHidePagesByDefaultForTest = true;
  settings_rtl_tests.registerDrawerPanelTests();
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/search_engines_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsSearchEnginesTest() {}

CrSettingsSearchEnginesTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/search_engines_page/search_engines_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'search_engines_page_test.js',
  ]),
};

TEST_F('CrSettingsSearchEnginesTest', 'SearchEngines', function() {
  settings_search_engines_page.registerDialogTests();
  settings_search_engines_page.registerSearchEngineEntryTests();
  settings_search_engines_page.registerOmniboxExtensionEntryTests();
  settings_search_engines_page.registerPageTests();
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
/**
 * Test fixture for device-page.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsDevicePageTest() {}

CrSettingsDevicePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/device_page/device_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'device_page_tests.js',
  ]),
};

TEST_F('CrSettingsDevicePageTest', 'DevicePage', function() {
  mocha.run();
});
GEN('#endif');
