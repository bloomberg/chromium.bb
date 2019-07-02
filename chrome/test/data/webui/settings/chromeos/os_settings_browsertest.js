// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Tests for Chrome OS settings page. */

// Path to general chrome browser settings and associated utilities.
const BROWSER_SETTINGS_PATH = '../';

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

GEN('#include "ash/public/cpp/ash_features.h"');
GEN('#include "chrome/common/chrome_features.h"');
GEN('#include "chromeos/constants/chromeos_features.h"');

// Generic text fixture for CrOS Polymer Settings elements to be overridden by
// individual element tests.
const OSSettingsBrowserTest = class extends PolymerTest {
  /** @override */
  get browsePreload() {
    return 'chrome://os-settings/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat(
        [BROWSER_SETTINGS_PATH + 'ensure_lazy_loaded.js']);
  }

  /** @override */
  setUp() {
    super.setUp();
    settings.ensureLazyLoaded('chromeos');
  }
};

// Test fixture for the Smb Shares page.
// eslint-disable-next-line no-var
var OSSettingsSmbPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_downloads_page/smb_shares_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'smb_shares_page_tests.js',
    ]);
  }
};

// Settings tests are flaky on debug. See https://crbug.com/968608.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_AllJavascriptTests DISABLED_AllJavascriptTests');
GEN('#else');
GEN('#define MAYBE_AllJavascriptTests AllJavascriptTests');
GEN('#endif');

TEST_F('OSSettingsSmbPageTest', 'MAYBE_AllJavascriptTests', function() {
  mocha.run();
});

// Test fixture for the chrome://os-settings/accounts page
// eslint-disable-next-line no-var
var OSSettingsAddUsersTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'accounts.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      'add_users_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAddUsersTest', 'MAYBE_AllJavascriptTests', function() {
  mocha.run();
});

// Test fixture for the main settings page.
// eslint-disable-next-line no-var
var OSSettingsMainTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload +
        'chromeos/os_settings_main/os_settings_main.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      'os_settings_main_test.js',
    ]);
  }
};

TEST_F('OSSettingsMainTest', 'MAYBE_AllJavascriptTests', function() {
  mocha.run();
});

// Tests for the side-nav menu.
// eslint-disable-next-line no-var
var OSSettingsMenuTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_settings_menu_test.js',
    ]);
  }
};

TEST_F('OSSettingsMenuTest', 'MAYBE_AllJavascriptTests', function() {
  mocha.run();
});

// Tests for the People section.
// eslint-disable-next-line no-var
var OSSettingsPeoplePageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'sync_test_util.js',
      BROWSER_SETTINGS_PATH + 'test_profile_info_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_sync_browser_proxy.js',
      'os_people_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsPeoplePageTest', 'MAYBE_AllJavascriptTests', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsAboutPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'chromeos/os_about_page/os_about_page.html';
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_about_page_browser_proxy.js',
      'os_about_page_tests.js',
    ]);
  }
};

TEST_F('OSSettingsAboutPageTest', 'AboutPage', function() {
  settings_about_page.registerTests();
  mocha.run();
});

GEN('#if defined(GOOGLE_CHROME_BUILD)');
TEST_F('OSSettingsAboutPageTest', 'AboutPage_OfficialBuild', function() {
  settings_about_page.registerOfficialBuildTests();
  mocha.run();
});
GEN('#endif');

// Tests for the Reset section.
// eslint-disable-next-line no-var
var OSSettingsResetPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + '../test_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_lifetime_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_reset_browser_proxy.js',
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_reset_page_test.js',
    ]);
  }
};

TEST_F('OSSettingsResetPageTest', 'AllJavascriptTests', function() {
  mocha.run();
});

// eslint-disable-next-line no-var
var OSSettingsAdvancedPageBrowserTest = class extends OSSettingsBrowserTest {
  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'test_util.js',
      'os_advanced_page_browsertest.js',
    ]);
  }
};

// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
TEST_F(
    'OSSettingsAdvancedPageBrowserTest', 'MAYBE_AllJavascriptTests',
    function() {
      // Run all registered tests.
      mocha.run();
    });
