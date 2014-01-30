// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for App Launcher's Settings App testing.
 * @extends {testing.Test}
 * @constructor
 */
function SettingsAppWebUITest() {}

SettingsAppWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to Settings App page, in a browser.
   */
  browsePreload: 'chrome://settings-frame/options_settings_app.html',
};

GEN('#if defined(ENABLE_SETTINGS_APP)');

// Test opening Settings App, and do some checks on section visibility.
TEST_F('SettingsAppWebUITest', 'testOpenSettingsApp', function() {
  // Note there is no location bar in the Settings App.

  // Some things are hidden via a parent, so make a helper function.
  function isVisible(elementId) {
    var elem = $(elementId);
    return elem.offsetWidth > 0 || elem.offsetHeight > 0;
  }
  assertTrue(OptionsPage.isSettingsApp());
  assertTrue(isVisible('sync-users-section'));
  assertTrue(isVisible('sync-section'));

  // Spot-check some regular settings items that should be hidden.
  assertFalse(isVisible('change-home-page-section'));
  assertFalse(isVisible('default-search-engine'));
  assertFalse(isVisible('hotword-search'));
  assertFalse(isVisible('privacy-section'));
  assertFalse(isVisible('startup-section'));
});

// Check functionality of LoadTimeData.overrideValues(), which the Settings App
// uses. Do spot checks, so the test is not too fragile. Some of the content
// strings rely on waiting for sync sign-in status, or platform-specifics.
TEST_F('SettingsAppWebUITest', 'testStrings', function() {
  // Ensure we check against the override values.
  assertTrue(!!loadTimeData.getValue('settingsApp'));

  // Check a product-specific label, to ensure it uses "App Launcher", and not
  // Chrome / Chromium.
  var languagesLabelElement =
      document.querySelector('[i18n-content="languageSectionLabel"]');
  assertNotEquals(-1, languagesLabelElement.innerHTML.indexOf('App Launcher'));
});

GEN('#endif  // defined(ENABLE_SETTINGS_APP)');
