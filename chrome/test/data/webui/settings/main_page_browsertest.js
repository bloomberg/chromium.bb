// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings main page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsMainPageBrowserTest() {}

SettingsMainPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype
};

TEST_F('SettingsMainPageBrowserTest', 'Main', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  var self = this;

  // Register mocha tests.
  suite('SettingsPage', function() {
    test('load page', function() {
      // This will fail if there are any asserts or errors in the Settings page.
    });

    test('basic pages', function() {
      var page = self.getPage('basic');
      expectTrue(!!self.getSection(page, 'appearance'));
      expectTrue(!!self.getSection(page, 'on-startup'));
      expectTrue(!!self.getSection(page, 'search'));
      if (!cr.isChromeOS) {
        expectTrue(!!self.getSection(page, 'people'));
        expectTrue(!!self.getSection(page, 'defaultBrowser'));
      } else {
        expectTrue(!!self.getSection(page, 'internet'));
        expectTrue(!!self.getSection(page, 'users'));
      }
    });

    test('advanced pages', function() {
      var page = self.getPage('advanced');
      expectTrue(!!self.getSection(page, 'location'));
      expectTrue(!!self.getSection(page, 'privacy'));
      expectTrue(!!self.getSection(page, 'passwordsAndForms'));
      expectTrue(!!self.getSection(page, 'languages'));
      expectTrue(!!self.getSection(page, 'downloads'));
      expectTrue(!!self.getSection(page, 'reset'));

      if (cr.isChromeOS) {
        expectTrue(!!self.getSection(page, 'dateTime'));
        expectTrue(!!self.getSection(page, 'bluetooth'));
        expectTrue(!!self.getSection(page, 'a11y'));
      }
    });
  });

  // Run all registered tests.
  mocha.run();
});
