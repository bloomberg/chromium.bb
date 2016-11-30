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
  get browsePreload() {
    throw 'this is abstract and should be overriden by subclasses';
  },

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    // We aren't loading the main document.
    this.accessibilityAuditConfig.ignoreSelectors('humanLangMissing', 'html');

    // TODO(michaelpg): Re-enable after bringing in fix for
    // https://github.com/PolymerElements/paper-slider/issues/131.
    this.accessibilityAuditConfig.ignoreSelectors(
        'badAriaAttributeValue', 'paper-slider');
  },
};

// Have to include command_line.h manually due to GEN calls below.
GEN('#include "base/command_line.h"');

function CrSettingsCheckboxTest() {}

CrSettingsCheckboxTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/controls/settings_checkbox.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'checkbox_tests.js',
  ]),
};

TEST_F('CrSettingsCheckboxTest', 'All', function() {
  settings_checkbox.registerTests();
  mocha.run();
});

function CrSettingsToggleButtonTest() {}

CrSettingsToggleButtonTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/controls/settings_toggle_button.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'settings_toggle_button_tests.js',
  ]),
};

TEST_F('CrSettingsToggleButtonTest', 'All', function() {
  settings_toggle_button.registerTests();
  mocha.run();
});

function CrSettingsDropdownMenuTest() {}

CrSettingsDropdownMenuTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/controls/settings_dropdown_menu.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'dropdown_menu_tests.js',
  ]),
};

TEST_F('CrSettingsDropdownMenuTest', 'All', function() {
  settings_dropdown_menu.registerTests();
  mocha.run();
});

function CrSettingsPrefUtilTest() {}

CrSettingsPrefUtilTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/prefs/pref_util.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'pref_util_tests.js',
  ]),
};

TEST_F('CrSettingsPrefUtilTest', 'All', function() {
  settings_prefUtil.registerTests();
  mocha.run();
});

function CrSettingsPrefsTest() {}

CrSettingsPrefsTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/prefs/prefs.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'prefs_test_cases.js',
    'prefs_tests.js',
  ]),
};

TEST_F('CrSettingsPrefsTest', 'All', function() {
  settings_prefs.registerTests();
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAboutPageTest() {}

CrSettingsAboutPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/about_page/about_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_util.js',
    'test_browser_proxy.js',
    'test_lifetime_browser_proxy.js',
    'about_page_tests.js',
  ]),
};

TEST_F('CrSettingsAboutPageTest', 'AboutPage', function() {
  settings_about_page.registerTests();
  mocha.run();
});

GEN('#if defined(GOOGLE_CHROME_BUILD)');
TEST_F('CrSettingsAboutPageTest', 'AboutPage_OfficialBuild', function() {
  settings_about_page.registerOfficialBuildTests();
  mocha.run();
});
GEN('#endif');

GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/password_prompt_dialog.html.
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageQuickUnlockAuthenticateTest() {}

CrSettingsPeoplePageQuickUnlockAuthenticateTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/password_prompt_dialog.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_quick_unlock_private.js',
    'quick_unlock_authenticate_browsertest_chromeos.js'
  ]),
};

TEST_F('CrSettingsPeoplePageQuickUnlockAuthenticateTest', 'Test', function() {
  settings_people_page_quick_unlock.registerAuthenticateTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/lock_screen.html
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageLockScreenTest() {}

CrSettingsPeoplePageLockScreenTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  //browsePreload: 'chrome://md-settings/settings_main/settings_main.html',
  browsePreload: 'chrome://md-settings/people_page/lock_screen.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_quick_unlock_private.js',
    'fake_settings_private.js',
    'quick_unlock_authenticate_browsertest_chromeos.js'
  ]),
};

TEST_F('CrSettingsPeoplePageLockScreenTest', 'Test', function() {
  settings_people_page_quick_unlock.registerLockScreenTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/setup_pin_dialog.html.
 *
 * This is ChromeOS only.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageSetupPinDialogTest() {}

CrSettingsPeoplePageSetupPinDialogTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/setup_pin_dialog.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_quick_unlock_private.js',
    'fake_settings_private.js',
    'quick_unlock_authenticate_browsertest_chromeos.js'
  ]),
};

TEST_F('CrSettingsPeoplePageSetupPinDialogTest', 'Test', function() {
  settings_people_page_quick_unlock.registerSetupPinDialogTests();
  mocha.run();
});

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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'people_page_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageTest', 'PeoplePage', function() {
  settings_people_page.registerTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/sync_page.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPeoplePageSyncPageTest() {}

CrSettingsPeoplePageSyncPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/sync_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'people_page_sync_page_test.js',
  ]),
};

TEST_F('CrSettingsPeoplePageSyncPageTest', 'SyncPage', function() {
  settings_people_page_sync_page.registerTests();
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'test_lifetime_browser_proxy.js',
    'reset_page_test.js',
  ]),
};

TEST_F('CrSettingsResetPageTest', 'ResetPage', function() {
  settings_reset_page.registerTests();
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAppearancePageTest() {}

CrSettingsAppearancePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/appearance_page/appearance_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'test_browser_proxy.js',
    'appearance_page_test.js',
  ]),
};

TEST_F('CrSettingsAppearancePageTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsAppearanceFontsPageTest() {}

CrSettingsAppearanceFontsPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/appearance_page/appearance_fonts_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'test_browser_proxy.js',
    'appearance_fonts_page_test.js',
  ]),
};

TEST_F('CrSettingsAppearanceFontsPageTest', 'All', function() {
  mocha.run();
});

GEN('#if !defined(OS_CHROMEOS)');
/**
 * Test fixture for chrome/browser/resources/settings/default_browser_page/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsDefaultBrowserTest() {}

CrSettingsDefaultBrowserTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/default_browser_page/default_browser_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'default_browser_browsertest.js',
  ]),
};

TEST_F('CrSettingsDefaultBrowserTest', 'DefaultBrowserPage', function() {
  settings_default_browser.registerTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/people_page/import_data_dialog.html
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsImportDataDialogTest() {}

CrSettingsImportDataDialogTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people_page/import_data_dialog.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'import_data_dialog_test.js',
  ]),
};

TEST_F('CrSettingsImportDataDialogTest', 'All', function() {
  mocha.run();
});
GEN('#endif');

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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'test_extension_control_browser_proxy.js',
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_util.js',
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    ROOT_PATH + 'ui/webui/resources/js/promise_resolver.js',
    'test_util.js',
    'test_browser_proxy.js',
    'test_privacy_page_browser_proxy.js',
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'site_details_tests.js',
    'site_details_permission_tests.js',
    'site_list_tests.js',
    'site_settings_category_tests.js',
    'test_browser_proxy.js',
    'test_site_settings_prefs_browser_proxy.js',
    'zoom_levels_tests.js',
    'usb_devices_tests.js',
    'protocol_handlers_tests.js'
  ]),
};

TEST_F('CrSettingsSiteSettingsTest', 'SiteSettings', function() {
  site_details.registerTests();
  site_details_permission.registerTests();
  site_list.registerTests();
  site_settings_category.registerTests();
  zoom_levels.registerTests();
  usb_devices.registerTests();
  protocol_handlers.registerTests();

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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    ROOT_PATH + 'ui/webui/resources/js/assert.js',
    '../fake_chrome_event.js',
    'fake_settings_private.js',
    'fake_system_display.js',
    'device_page_tests.js',
  ]),
};

TEST_F('CrSettingsDevicePageTest', 'DevicePageTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.DevicePage)).run();
});

TEST_F('CrSettingsDevicePageTest', 'DisplayTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Display)).run();
});

TEST_F('CrSettingsDevicePageTest', 'KeyboardTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Keyboard)).run();
});

TEST_F('CrSettingsDevicePageTest', 'PointersTest', function() {
  mocha.grep(assert(device_page_tests.TestNames.Pointers)).run();
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
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'settings_menu_test.js',
  ]),
};

TEST_F('CrSettingsMenuTest', 'SettingsMenu', function() {
  settings_menu.registerTests();
  mocha.run();
});

/**
 * Test fixture for
 * chrome/browser/resources/settings/settings_page/settings_subpage.html.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSubpageTest() {}

CrSettingsSubpageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/settings_page/settings_subpage.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'settings_subpage_test.js',
  ]),
};

TEST_F('CrSettingsSubpageTest', 'SettingsSubpage', function() {
  settings_subpage.registerTests();
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
    'test_lifetime_browser_proxy.js',
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
function CrSettingsLanguagesTest() {}

CrSettingsLanguagesTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/languages_page/languages.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    '../fake_chrome_event.js',
    'fake_language_settings_private.js',
    'fake_settings_private.js',
    'languages_tests.js',
  ]),
};

TEST_F('CrSettingsLanguagesTest', 'Languages', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsRouteTest() {}

CrSettingsRouteTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/route.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'route_tests.js',
  ]),
};

TEST_F('CrSettingsRouteTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function CrSettingsNonExistentRouteTest() {}

CrSettingsNonExistentRouteTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/non/existent/route',

  /** @override */
  runAccessibilityChecks: false,
};

TEST_F('CrSettingsNonExistentRouteTest', 'All', function() {
  suite('NonExistentRoutes', function() {
    test('redirect to basic', function() {
      assertEquals(settings.Route.BASIC, settings.getCurrentRoute());
      assertEquals('/', location.pathname);
    });
  });
  mocha.run();
});

// Hangs on ASAN builder for unknown reasons. TODO(michaelpg): Find reason.
GEN('#if defined(ADDRESS_SANITIZER)');
GEN('#define MAYBE_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_All All');
GEN('#endif');

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function CrSettingsRouteDynamicParametersTest() {}

CrSettingsRouteDynamicParametersTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/people?guid=a%2Fb&foo=42',

  /** @override */
  runAccessibilityChecks: false,
};

TEST_F('CrSettingsRouteDynamicParametersTest', 'MAYBE_All', function() {
  suite('DynamicParameters', function() {
    test('get parameters from URL and navigation', function(done) {
      assertEquals(settings.Route.PEOPLE, settings.getCurrentRoute());
      assertEquals('a/b', settings.getQueryParameters().get('guid'));
      assertEquals('42', settings.getQueryParameters().get('foo'));

      var params = new URLSearchParams();
      params.set('bar', 'b=z');
      params.set('biz', '3');
      settings.navigateTo(settings.Route.SYNC, params);
      assertEquals(settings.Route.SYNC, settings.getCurrentRoute());
      assertEquals('b=z', settings.getQueryParameters().get('bar'));
      assertEquals('3', settings.getQueryParameters().get('biz'));
      assertEquals('?bar=b%3Dz&biz=3', window.location.search);

      window.addEventListener('popstate', function(event) {
        assertEquals('/people', settings.getCurrentRoute().path);
        assertEquals(settings.Route.PEOPLE, settings.getCurrentRoute());
        assertEquals('a/b', settings.getQueryParameters().get('guid'));
        assertEquals('42', settings.getQueryParameters().get('foo'));
        done();
      });
      window.history.back();
    });
  });
  mocha.run();
});

// Times out on Windows Tests (dbg). See https://crbug.com/651296.
// Times out / crashes on chromium.linux/Linux Tests (dbg) crbug.com/667882
GEN('#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_LINUX)');
GEN('#define MAYBE_MainPage_All DISABLED_All');
GEN('#else');
GEN('#define MAYBE_MainPage_All All');
GEN('#endif');

/**
 * Test fixture for chrome/browser/resources/settings/settings_main/.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsMainPageTest() {}

CrSettingsMainPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/settings_main/settings_main.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'settings_main_test.js',
  ]),
};

TEST_F('CrSettingsMainPageTest', 'MAYBE_MainPage_All', function() {
  settings_main_page.registerTests();
  mocha.run();
});

/**
 * Test fixture for chrome/browser/resources/settings/search_settings.js.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsSearchTest() {}

CrSettingsSearchTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/settings_page/settings_section.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'search_settings_test.js',
  ]),
};

TEST_F('CrSettingsSearchTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrControlledButtonTest() {}

CrControlledButtonTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/controls/controlled_button.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'controlled_button_tests.js',
  ]),
};

TEST_F('CrControlledButtonTest', 'All', function() {
  mocha.run();
});

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrControlledRadioButtonTest() {}

CrControlledRadioButtonTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/controls/controlled_radio_button.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'controlled_radio_button_tests.js',
  ]),
};

TEST_F('CrControlledRadioButtonTest', 'All', function() {
  mocha.run();
});

GEN('#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)');

function CrSettingsMetricsReportingTest() {}

CrSettingsMetricsReportingTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/privacy_page/privacy_page.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'test_privacy_page_browser_proxy.js',
    'metrics_reporting_tests.js',
  ]),
};

TEST_F('CrSettingsMetricsReportingTest', 'All', function() {
  mocha.run();
});

GEN('#endif');
GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for the CUPS printing page.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsPrintingPageTest() {}

CrSettingsPrintingPageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/printing_page/cups_add_printer_dialog.html',

  /** @override */
  commandLineSwitches: [{
    switchName: 'enable-native-cups',
  }],

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    ROOT_PATH + 'ui/webui/resources/js/assert.js',
    'test_util.js',
    'test_browser_proxy.js',
    'cups_printer_page_tests.js',
  ]),
};

TEST_F('CrSettingsPrintingPageTest', 'CupsPrintersTest', function() {
  mocha.run();
});

GEN('#endif');
GEN('#if defined(OS_CHROMEOS)');

/**
 * Test fixture for the Date and Time page.
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsDateTimePageTest() {}

CrSettingsDateTimePageTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://md-settings/date_time_page/date_time_page.html',

  /** @override */
  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'date_time_page_tests.js',
  ]),
};

TEST_F('CrSettingsDateTimePageTest', 'DateTimePageTest', function() {
  mocha.run();
});

GEN('#endif');

/**
 * @constructor
 * @extends {CrSettingsBrowserTest}
 */
function CrSettingsExtensionControlledIndicatorTest() {}

CrSettingsExtensionControlledIndicatorTest.prototype = {
  __proto__: CrSettingsBrowserTest.prototype,

  /** @override */
  browsePreload:
      'chrome://md-settings/controls/extension_controlled_indicator.html',

  extraLibraries: CrSettingsBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
    'test_extension_control_browser_proxy.js',
    'extension_controlled_indicator_tests.js',
  ]),
};

TEST_F('CrSettingsExtensionControlledIndicatorTest', 'All', function() {
  mocha.run();
});
