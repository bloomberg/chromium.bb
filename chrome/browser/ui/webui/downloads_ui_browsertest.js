// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['downloads_ui_browsertest_base.js']);
GEN('#include "chrome/browser/ui/webui/downloads_ui_browsertest.h"');

// Test UI when removing entries is allowed.
TEST_F('BaseDownloadsWebUITest', 'DeleteAllowed', function() {
  this.testHelper(true, false);
  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
  testDone();
});

TEST_F('BaseDownloadsWebUITest', 'NoResultsHiddenWhenDownloads', function() {
  assertNotEquals(0, downloads.size());
  expectFalse($('downloads-display').hidden);
  expectTrue($('no-downloads-or-results').hidden);
});

/**
 * @constructor
 * @extends {BaseDownloadsWebUITest}
 */
function EmptyDownloadsWebUITest() {}

EmptyDownloadsWebUITest.prototype = {
  __proto__: BaseDownloadsWebUITest.prototype,

  /** @override */
  setUp: function() {
    // Doesn't create any fake downloads.
    assertEquals(0, downloads.size());
  },
};

TEST_F('EmptyDownloadsWebUITest', 'NoDownloadsMessageShowing', function() {
  expectTrue($('downloads-display').hidden);
  expectFalse($('no-downloads-or-results').hidden);
});

/**
 * Fixture for Downloads WebUI testing when deletions are prohibited.
 * @extends {BaseDownloadsWebUITest}
 * @constructor
 */
function DownloadsWebUIDeleteProhibitedTest() {}

DownloadsWebUIDeleteProhibitedTest.prototype = {
  __proto__: BaseDownloadsWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    GEN('  SetDeleteAllowed(false);');
  },
};

// Test UI when removing entries is prohibited.
TEST_F('DownloadsWebUIDeleteProhibitedTest', 'DeleteProhibited', function() {
  this.testHelper(false, false);
  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
  testDone();
});
