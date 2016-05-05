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

  // Run all registered tests.
  mocha.run();
});

GEN('#if defined(OS_CHROMEOS)');
/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/change_picture.html.
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsPeoplePageChangePictureTest() {}

CrSettingsPeoplePageChangePictureTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/change_picture.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_browser_proxy.js',
    'people_page_change_picture_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageChangePictureTest', 'ChangePicture', function() {
  settings_people_page_change_picture.registerTests();
  mocha.run();
});
GEN('#else');  // !defined(OS_CHROMEOS)
/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/manage_profile.html.
 * This is non-ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsPeoplePageManageProfileTest() {}

CrSettingsPeoplePageManageProfileTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/manage_profile.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_browser_proxy.js',
    'people_page_manage_profile_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageManageProfileTest', 'ManageProfile', function() {
  settings_people_page_manage_profile.registerTests();
  mocha.run();
});
GEN('#endif');

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/people_page.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsPeoplePageTest() {}

CrSettingsPeoplePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/people_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_browser_proxy.js',
    'people_page_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageTest', 'PeoplePage', function() {
  settings_people_page.registerTests();
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
 * Test fixture for chrome/browser/resources/settings/reset_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsResetPageTest() {}

CrSettingsResetPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/reset_page/reset_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_browser_proxy.js',
    'reset_page_test.js',
  ]),
};

TEST_F('CrSettingsResetPageTest', 'ResetPage', function() {
  settings_reset_page.registerTests();
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/appearance_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsAppearancePageTest() {}

CrSettingsAppearancePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/appearance_page/appearance_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'test_browser_proxy.js',
    'appearance_page_test.js',
  ]),
};

TEST_F('CrSettingsAppearancePageTest', 'AppearancePage', function() {
  settings_appearance.registerTests();
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/search_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsSearchPageTest() {}

CrSettingsSearchPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/search_page/search_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_browser_proxy.js',
    'test_search_engines_browser_proxy.js',
    'search_page_test.js',
  ]),
};

TEST_F('CrSettingsSearchPageTest', 'SearchPage', function() {
  settings_search_page.registerTests();
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
    'test_browser_proxy.js',
    'test_search_engines_browser_proxy.js',
    'search_engines_page_test.js',
  ]),
};

TEST_F('CrSettingsSearchEnginesTest', 'SearchEngines', function() {
  settings_search_engines_page.registerTests();
  mocha.run();
});

GEN('#if defined(USE_NSS_CERTS)');
/**
 * Test fixture for chrome/browser/resources/settings/certificate_manager_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsCertificateManagerTest() {}

CrSettingsCertificateManagerTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/certificate_manager_page/' +
      'certificate_manager_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'test_browser_proxy.js',
    'certificate_manager_page_test.js',
  ]),
};

TEST_F('CrSettingsCertificateManagerTest', 'CertificateManager', function() {
  certificate_manager_page.registerTests();
  mocha.run();
});
GEN('#endif');

/**
 * Test fixture for chrome/browser/resources/settings/privacy_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsPrivacyPageTest() {}

CrSettingsPrivacyPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/privacy_page/privacy_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'test_browser_proxy.js',
    'privacy_page_test.js',
  ]),
};

TEST_F('CrSettingsPrivacyPageTest', 'PrivacyPage', function() {
  settings_privacy_page.registerTests();
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/site_settings/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsSiteSettingsTest() {}

CrSettingsSiteSettingsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/prefs/prefs.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'site_details_tests.js',
    'site_details_permission_tests.js',
    'site_list_tests.js',
    'site_settings_category_tests.js',
    'test_browser_proxy.js',
    'test_site_settings_prefs_browser_proxy.js',
  ]),
};

TEST_F('CrSettingsSiteSettingsTest', 'SiteSettings', function() {
  site_details.registerTests();
  site_details_permission.registerTests();
  site_list.registerTests();
  site_settings_category.registerTests();

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
  browsePreload: 'chrome://md-settings/device_page/device_page.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'fake_system_display.js',
    'device_page_tests.js',
  ]),
};

TEST_F('CrSettingsDevicePageTest', 'DevicePage', function() {
  mocha.run();
});
GEN('#endif');

/**
 * Test fixture for chrome/browser/resources/settings/settings_menu/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsMenuTest() {}

CrSettingsMenuTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/settings_menu/settings_menu.html',

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'settings_menu_test.js',
  ]),
};

TEST_F('CrSettingsMenuTest', 'SettingsMenu', function() {
  settings_menu.registerTests();
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');
/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
*/
function CrSettingsSystemPageTest() {}

CrSettingsSystemPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/system_page/system_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'system_page_tests.js',
  ]),
};

TEST_F('CrSettingsSystemPageTest', 'Restart', function() {
  mocha.run();
});
GEN('#endif');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsStartupUrlsPageTest() {}

CrSettingsStartupUrlsPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  browsePreload: 'chrome://md-settings/on_startup_page/startup_urls_page.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'startup_urls_page_test.js',
  ]),
};

TEST_F('CrSettingsStartupUrlsPageTest', 'StartupUrlsPage', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsLanguagesPageTest() {}

CrSettingsLanguagesPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/languages_page/languages_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'fake_language_settings_private.js',
    'fake_settings_private.js',
    'languages_page_tests.js',
  ]),
};

TEST_F('CrSettingsLanguagesPageTest', 'LanguagesPage', function() {
  mocha.run();
});
