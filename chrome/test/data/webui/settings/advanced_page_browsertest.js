// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings advanced page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsAdvancedPageBrowserTest() {}

SettingsAdvancedPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');

TEST_F('SettingsAdvancedPageBrowserTest', 'MAYBE_Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  var self = this;

  // Register mocha tests.
  suite('SettingsPage', function() {
    suiteSetup(function() {
      self.toggleAdvanced();
    });

    test('load page', function() {
      // This will fail if there are any asserts or errors in the Settings page.
    });

    test('advanced pages', function() {
      var page = self.getPage('basic');
      var sections = ['privacy', 'passwordsAndForms', 'languages', 'downloads',
          'reset'];
      if (cr.isChromeOS)
        sections = sections.concat(['dateTime', 'a11y']);

      for (var i = 0; i < sections.length; i++) {
        var section = self.getSection(page, sections[i]);
        expectTrue(!!section);
        self.verifySubpagesHidden(section);
      }
    });
  });

  // Run all registered tests.
  mocha.run();
});
