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
  suite('AppearanceHandler', function() {
    test('font settings', function() {
      var fontSize = self.appearancePage(
          '#defaultFontSize').$$('[class=iron-selected]');
      assertEquals('Medium', fontSize.textContent);
    });

    test('home button', function() {
      var homeButton = self.appearancePage('#showHomeButton');
      assertEquals('false', homeButton.getAttribute('aria-pressed'));
    });
  });
  mocha.run();
});
