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
    return 'chrome://settings/chromeos/';
  }

  /** @override */
  get featureList() {
    return {enabled: ['chromeos::features::kSplitSettings']};
  }

  /** @override */
  get extraLibraries() {
    return super.extraLibraries.concat([
      BROWSER_SETTINGS_PATH + 'ensure_lazy_loaded.js',
    ]);
  }

  /** @override */
  setUp() {
    super.setUp();
    settings.ensureLazyLoaded();
  }
};

// Test fixture for the Smb Shares page.
OSSettingsSmbPageTest = class extends OSSettingsBrowserTest {
  /** @override */
  get browsePreload() {
    return super.browsePreload + 'os_downloads_page/smb_shares_page.html';
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

TEST_F('OSSettingsSmbPageTest', 'All', function() {
  mocha.run();
});
