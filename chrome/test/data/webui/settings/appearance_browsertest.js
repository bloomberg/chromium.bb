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
  var self = this;

  var fontSize = function() {
    return self.appearancePage(
        '#defaultFontSize').$$('[class=iron-selected]').textContent.trim();
  };

  suite('AppearanceHandler', function() {
    test('font settings', function(done) {
      chrome.settingsPrivate.setPref(
          'webkit.webprefs.default_font_size', 16, '', function(result) {
        assertTrue(result);
        assertEquals('Medium', fontSize());
        chrome.settingsPrivate.setPref(
            'webkit.webprefs.default_font_size', 20, '', function(result) {
          assertTrue(result);
          chrome.settingsPrivate.getPref(
                'webkit.webprefs.default_font_size', function(pref) {
              assertEquals(pref.value, 20);
              assertEquals('Large', fontSize());
              done();
          });
        });
      });
    });

    /**
     * If the font size is not one of the preset options (e.g. 16, 20, etc.)
     * then the menu label will be 'Custom' (rather than 'Medium', 'Large',
     * etc.).
     */
    test('font size custom', function(done) {
      chrome.settingsPrivate.setPref(
          'webkit.webprefs.default_font_size', 19, '', function(result) {
        assertTrue(result);
        assertEquals('Custom', fontSize());
        done();
      });
    });

    test('home button', function() {
      var homeButton = self.appearancePage('#showHomeButton');
      assertEquals('false', homeButton.getAttribute('aria-pressed'));
    });
  });
  mocha.run();
});
