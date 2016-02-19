// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings Device page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
*/
function SettingsDevicePageBrowserTest() {}

SettingsDevicePageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_DevicePage DISABLED_DevicePage');
GEN('#else');
GEN('#define MAYBE_DevicePage DevicePage');
GEN('#endif');

TEST_F('SettingsDevicePageBrowserTest', 'MAYBE_DevicePage', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  var self = this;

  // Register mocha tests.
  suite('SettingsDevicePage', function() {
    test('device section', function() {
      var page = self.getPage('basic');
      var deviceSection = self.getSection(page, 'device');
      expectTrue(!!deviceSection);
    });
  });

  // Run all registered tests.
  mocha.run();
});
