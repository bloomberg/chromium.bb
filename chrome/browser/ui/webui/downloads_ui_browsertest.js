// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['downloads_ui_browsertest_base.js']);
GEN('#include "chrome/browser/ui/webui/downloads_ui_browsertest.h"');

// Test UI when removing entries is allowed.
TEST_F('BaseDownloadsWebUITest', 'DeleteAllowed', function() {
  this.expectDeleteControlsVisible(true);
  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
});

TEST_F('BaseDownloadsWebUITest', 'NoResultsHiddenWhenDownloads', function() {
  assertNotEquals(0, downloads.Manager.size());
  expectFalse($('downloads-display').hidden);
  expectTrue($('no-downloads-or-results').hidden);
});

TEST_F('BaseDownloadsWebUITest', 'NoSearchResultsShown', function() {
  expectFalse($('downloads-display').hidden);
  var noResults = $('no-downloads-or-results');
  expectTrue(noResults.hidden);

  downloads.Manager.setSearchText('just try to search for me!');
  this.sendEmptyList();

  expectTrue($('downloads-display').hidden);
  this.checkShowing(noResults, loadTimeData.getString('no_search_results'));
});

TEST_F('BaseDownloadsWebUITest', 'NoDownloadsAfterClearAll', function() {
  expectFalse($('downloads-display').hidden);
  var noResults = $('no-downloads-or-results');
  expectTrue(noResults.hidden);

  $('clear-all').click();
  this.sendEmptyList();

  expectTrue($('downloads-display').hidden);
  this.checkShowing(noResults, loadTimeData.getString('no_downloads'));
});

TEST_F('BaseDownloadsWebUITest', 'PauseResumeFocus', function() {
  var manager = downloads.Manager.getInstance();
  assertGE(manager.size(), 0);

  var lastId = manager.items_.slice(-1)[0].view.id_;
  var freshData = this.createDownload(lastId, Date.now());
  freshData.state = downloads.Item.States.IN_PROGRESS;
  freshData.resume = false;
  downloads.Manager.updateItem(freshData);

  var node = manager.idMap_[lastId].view.node;
  var pause = node.querySelector('.pause');
  var resume = node.querySelector('.resume');

  expectFalse(pause.hidden);
  expectTrue(resume.hidden);
  // Move the focus to "Pause" then pretend the download was resumed. The focus
  // should move to the equivalent button ("Resume" in this case).
  pause.focus();
  assertEquals(document.activeElement, pause);

  freshData.state = downloads.Item.States.PAUSED;
  freshData.resume = true;
  downloads.Manager.updateItem(freshData);

  expectTrue(pause.hidden);
  expectFalse(resume.hidden);
  expectEquals(document.activeElement, resume);
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
    assertEquals(0, downloads.Manager.size());
  },
};

TEST_F('EmptyDownloadsWebUITest', 'NoDownloadsMessageShowing', function() {
  expectTrue($('downloads-display').hidden);
  var noResults = $('no-downloads-or-results');
  this.checkShowing(noResults, loadTimeData.getString('no_downloads'));
});

TEST_F('EmptyDownloadsWebUITest', 'NoSearchResultsWithNoDownloads', function() {
  downloads.Manager.setSearchText('bananas');
  this.sendEmptyList();

  expectTrue($('downloads-display').hidden);
  var noResults = $('no-downloads-or-results');
  this.checkShowing(noResults, loadTimeData.getString('no_search_results'));
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
  this.expectDeleteControlsVisible(false);
  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
});

TEST_F('DownloadsWebUIDeleteProhibitedTest', 'ClearLeavesSearch', function() {
  downloads.Manager.setSearchText('muhahaha');
  $('clear-all').click();
  expectGE(downloads.Manager.getInstance().searchText_.length, 0);
});
