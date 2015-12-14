// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs Polymer Appearance Settings elements. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * Test Polymer Appearance Settings elements.
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function AppearanceSettingsBrowserTest() {}

AppearanceSettingsBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    'fake_settings_private.js',
  ]),

  /** @return {string} */
  appearancePage: function(selector) {
    var section = this.getSection(this.getPage('basic'), 'appearance');
    assertTrue(!!section);
    var appearance = section.querySelector('settings-appearance-page');
    assertTrue(!!appearance);
    var result = appearance.$$(selector);
    assertTrue(!!result);
    return result;
  },
};

TEST_F('AppearanceSettingsBrowserTest', 'uiTests', function() {
  /**
   * The prefs API that will get a fake implementation.
   * @type {!SettingsPrivate}
   */
  var settingsPrefs;
  var self = this;

  var fontSize = function() {
    return self.appearancePage(
        '#defaultFontSize').$$('[class=iron-selected]').textContent.trim();
  };

  suite('AppearanceHandler', function() {
    var fakePrefs = [{
      key: 'webkit.webprefs.default_font_size',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 1234,
    }];

    suiteSetup(function() {
      settingsPrefs = document.querySelector(
          'cr-settings').$$('settings-prefs');
      assertTrue(!!settingsPrefs);
      CrSettingsPrefs.resetForTesting();
      settingsPrefs.resetForTesting();
      var fakeApi = new settings.FakeSettingsPrivate(fakePrefs);
      settingsPrefs.initializeForTesting(fakeApi);
      return CrSettingsPrefs.initialized;
    });

    test('very small font', function() {
      settingsPrefs.set('prefs.webkit.webprefs.default_font_size.value', 9);
      assertEquals('Very small', fontSize());
    });

    test('large font', function() {
      settingsPrefs.set('prefs.webkit.webprefs.default_font_size.value', 20);
      assertEquals('Large', fontSize());
    });

    /**
     * If the font size is not one of the preset options (e.g. 16, 20, etc.)
     * then the menu label will be 'Custom' (rather than 'Medium', 'Large',
     * etc.).
     */
    test('custom font size', function() {
      settingsPrefs.set(
          'prefs.webkit.webprefs.default_font_size', 19);
      assertEquals('Custom', fontSize());
    });
  });
  mocha.run();
});
