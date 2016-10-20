// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Material Help page tests. */

GEN_INCLUDE(['settings_page_browsertest.js']);
GEN('#include "base/command_line.h"');

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsHelpPageBrowserTest() {}

SettingsHelpPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  browsePreload: 'chrome://help/',

  commandLineSwitches: [{switchName: 'enable-features',
                         switchValue: 'MaterialDesignSettings'}],

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

TEST_F('SettingsHelpPageBrowserTest', 'Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  var self = this;

  // Register mocha tests.
  suite('Help page', function() {
    test('about section', function() {
      var page = self.getPage('about');
      expectTrue(!!self.getSection(page, 'about'));
    });
  });

  // Run all registered tests.
  mocha.run();
});
