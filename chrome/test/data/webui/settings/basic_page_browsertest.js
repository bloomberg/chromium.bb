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
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
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
      var sections = ['appearance', 'onStartup', 'people', 'search'];
      expectTrue(!!self.getSection(page, 'appearance'));
      if (!cr.isChromeOS)
        sections.push('defaultBrowser');
      else
        sections = sections.concat(['internet', 'device']);

      for (var i = 0; i < sections.length; i++) {
        var section = self.getSection(page, sections[i]);
        expectTrue(!!section);
        self.verifySubpagesHidden(section);
      }
    });

    // This test checks for a regression that occurred with scrollToSection_
    // failing to find its host element.
    test('scroll to section', function() {
      // Setting the page and section will cause a scrollToSection_.
      self.getPage('basic').currentRoute = {
        page: 'basic',
        section: 'onStartup',
        subpage: [],
      };

      return new Promise(function(resolve, reject) {
        var intervalId = window.setInterval(function() {
          var page = self.getPage('basic');
          if (self.getSection(page, page.currentRoute.section)) {
            window.clearInterval(intervalId);
            resolve();
          }
        }, 55);
      }.bind(self));
    });
  });

  // Run all registered tests.
  mocha.run();
});
