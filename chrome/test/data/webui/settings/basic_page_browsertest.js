// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsBasicPageBrowserTest() {}

SettingsBasicPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Main');
GEN('#else');
GEN('#define MAYBE_Load Main');
GEN('#endif');

TEST_F('SettingsBasicPageBrowserTest', 'MAYBE_Load', function() {
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
      expectTrue(!!self.getSection(page, 'onStartup'));
      expectTrue(!!self.getSection(page, 'people'));
      expectTrue(!!self.getSection(page, 'search'));
      if (!cr.isChromeOS) {
        expectTrue(!!self.getSection(page, 'defaultBrowser'));
      } else {
        expectTrue(!!self.getSection(page, 'internet'));
      }
    });
  });

  // Run all registered tests.
  mocha.run();
});
