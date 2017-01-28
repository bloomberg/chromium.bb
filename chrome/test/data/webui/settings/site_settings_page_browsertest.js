// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Site settings page tests. */

GEN_INCLUDE(['settings_page_browsertest.js']);
/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsSiteSettingsPageBrowserTest() {}

SettingsSiteSettingsPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,
};

TEST_F('SettingsSiteSettingsPageBrowserTest', 'labels', function() {
  suite('Site settings page', function() {
    var ui;

    suiteSetup(function() {
      ui = assert(document.createElement('settings-site-settings-page'));
    });

    test('defaultSettingLabel_ tests', function() {
      assertEquals('a', ui.defaultSettingLabel_(
          settings.PermissionValues.ALLOW, 'a', 'b'));
      assertEquals('b', ui.defaultSettingLabel_(
          settings.PermissionValues.BLOCK, 'a', 'b'));
      assertEquals('a', ui.defaultSettingLabel_(
          settings.PermissionValues.ALLOW, 'a', 'b', 'c'));
      assertEquals('b', ui.defaultSettingLabel_(
          settings.PermissionValues.BLOCK, 'a', 'b', 'c'));
      assertEquals('c', ui.defaultSettingLabel_(
          settings.PermissionValues.SESSION_ONLY, 'a', 'b', 'c'));
      assertEquals('c', ui.defaultSettingLabel_(
          settings.PermissionValues.DEFAULT, 'a', 'b', 'c'));
      assertEquals('c', ui.defaultSettingLabel_(
          settings.PermissionValues.ASK, 'a', 'b', 'c'));
      assertEquals('c', ui.defaultSettingLabel_(
          settings.PermissionValues.DETECT_IMPORTANT_CONTENT, 'a', 'b', 'c'));
    });
  });

  mocha.run();
});
