// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/browser/ui/webui/downloads_ui_browsertest.h"');

/** @const */ var TOTAL_RESULT_COUNT = 25;

/**
 * Test C++ fixture for downloads WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function DownloadsUIBrowserTest() {}

/**
 * Base fixture for Downloads WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function BaseDownloadsWebUITest() {}

BaseDownloadsWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the downloads page & call our preLoad().
   */
  browsePreload: 'chrome://downloads',

  /** @override */
  typedefCppFixture: 'DownloadsUIBrowserTest',

  /** @override */
  testGenPreamble: function() {
    GEN('  SetDeleteAllowed(true);');
  },

  /**
   * Sends TOTAL_RESULT_COUNT fake downloads to the page. This can't be called
   * in the preLoad, because it requires the global Download object to have
   * been created by the page.
   * @override
   */
  setUp: function() {
    // The entries will begin at 1:00 AM on Sept 2, 2008, and will be spaced
    // two minutes apart.
    var timestamp = new Date(2008, 9, 2, 1, 0).getTime();
    for (var i = 0; i < TOTAL_RESULT_COUNT; ++i) {
      downloads.updated(this.createDownload_(i, timestamp));
      timestamp += 2 * 60 * 1000;  // Next visit is two minutes later.
    }
    expectEquals(downloads.size(), TOTAL_RESULT_COUNT);
  },

  /**
   * Creates a download object to be passed to the page, following the expected
   * backend format (see downloads_dom_handler.cc).
   * @param {Number} A unique ID for the download.
   * @param {Number} The time the download purportedly started.
   */
  createDownload_: function(id, timestamp) {
    var download = {};
    download.id = id;
    download.started = timestamp;
    download.otr = false;
    download.state = Download.States.COMPLETE;
    download.retry = false;
    download.file_path = '/path/to/file';
    download.file_url = 'http://google.com/' + timestamp;
    download.file_name = 'download_' + timestamp;
    download.url = 'http://google.com/' + timestamp;
    download.file_externally_removed = false;
    download.danger_type = Download.DangerType.NOT_DANGEROUS;
    download.last_reason_text = '';
    download.since_string = 'today';
    download.date_string = 'today';
    download.percent = 100;
    download.progress_status_text = 'done';
    download.received = 128;

    return download;
  },
};

// Test UI when removing entries is allowed.
TEST_F('BaseDownloadsWebUITest', 'deleteAllowed', function() {
  // "Clear all" should be a link.
  var clearAllHolder = document.querySelectorAll('#clear-all-holder > a');
  expectEquals(clearAllHolder.length, 1);

  // All the "Remove from list" items should be links.
  var removeLinks = document.querySelectorAll(
      '.controls > a.control-remove-link');
  expectEquals(removeLinks.length, TOTAL_RESULT_COUNT);

  // There should be no disabled text "links".
  var disabledLinks = document.querySelectorAll('.disabled-link');
  expectEquals(disabledLinks.length, 0);

  // The model is updated synchronously, even though the actual back-end removal
  // (tested elsewhere) is asynchronous.
  clearAll();
  expectEquals(downloads.size(), 0);

  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
  testDone();
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
TEST_F('DownloadsWebUIDeleteProhibitedTest', 'deleteProhibited', function() {
  // "Clear all" should not be a link.
  var clearAllText = document.querySelectorAll(
      '#clear-all-holder.disabled-link');
  expectEquals(clearAllText.length, 1);
  expectEquals(clearAllText[0].nodeName, 'SPAN');

  // There should be no "Clear all" link.
  var clearAllLinks = document.querySelectorAll('clear-all-link');
  expectEquals(clearAllLinks.length, 0);

  // All the "Remove from list" items should be text. Check only one, to avoid
  // spam in case of failure.
  var removeTexts = document.querySelectorAll('.controls > .disabled-link');
  expectEquals(removeTexts.length, TOTAL_RESULT_COUNT);
  expectEquals(removeTexts[0].nodeName, 'SPAN');

  // There should be no "Remove from list" links.
  var removeLinks = document.querySelectorAll('control-remove-link');
  expectEquals(removeLinks.length, 0);

  // Attempting to remove items anyway should fail.
  // The model would have been cleared synchronously, even though the actual
  // back-end removal (also disabled, but tested elsewhere) is asynchronous.
  clearAll();
  expectEquals(downloads.size(), TOTAL_RESULT_COUNT);

  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
  testDone();
});
