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
  browsePreload: 'chrome://downloads/',

  /** @override */
  typedefCppFixture: 'DownloadsUIBrowserTest',

  /** @override */
  testGenPreamble: function() {
    GEN('  SetDeleteAllowed(true);');
  },

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

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
   * @param {number} A unique ID for the download.
   * @param {number} The time the download purportedly started.
   * @return {!Object} A fake download object.
   * @private
   */
  createDownload_: function(id, timestamp) {
    return {
      id: id,
      started: timestamp,
      otr: false,
      state: Download.States.COMPLETE,
      retry: false,
      file_path: '/path/to/file',
      file_url: 'http://google.com/' + timestamp,
      file_name: 'download_' + timestamp,
      url: 'http://google.com/' + timestamp,
      file_externally_removed: false,
      danger_type: Download.DangerType.NOT_DANGEROUS,
      last_reason_text: '',
      since_string: 'today',
      date_string: 'today',
      percent: 100,
      progress_status_text: 'done',
      received: 128,
    };
  },

  /**
   * Asserts the correctness of the state of the UI elements
   * that delete the download history.
   * @param {boolean} allowDelete True if download history deletion is
   *     allowed and false otherwise.
   * @param {boolean} expectControlsHidden True if the controls to delete
   *     download history are expected to be hidden and false otherwise.
   */
  testHelper: function(allowDelete, expectControlsHidden) {
    var clearAllElements = document.getElementsByClassName('clear-all-link');
    var disabledElements = document.getElementsByClassName('disabled-link');
    var removeLinkElements =
        document.getElementsByClassName('control-remove-link');

    // "Clear all" should be a link only when deletions are allowed.
    expectEquals(allowDelete ? 1 : 0, clearAllElements.length);

    // There should be no disabled links when deletions are allowed.
    // On the other hand, when deletions are not allowed, "Clear All"
    // and all "Remove from list" links should be disabled.
    expectEquals(allowDelete ? 0 : TOTAL_RESULT_COUNT + 1,
        disabledElements.length);

    // All "Remove from list" items should be links when deletions are allowed.
    // On the other hand, when deletions are not allowed, all
    // "Remove from list" items should be text.
    expectEquals(allowDelete ? TOTAL_RESULT_COUNT : 0,
        removeLinkElements.length);

    if (allowDelete) {
      // "Clear all" should not be hidden.
      expectFalse(clearAllElements[0].hidden);

      // No "Remove from list" items should be hidden.
      expectFalse(removeLinkElements[0].hidden);
    } else {
      expectEquals(expectControlsHidden, disabledElements[0].hidden);
    }

    // The model is updated synchronously, even though the actual
    // back-end removal (tested elsewhere) is asynchronous.
    clearAll();
    expectEquals(allowDelete ? 0 : TOTAL_RESULT_COUNT, downloads.size());
  },
};

// Test UI when removing entries is allowed.
TEST_F('BaseDownloadsWebUITest', 'DeleteAllowed', function() {
  this.testHelper(true, false);
  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
  testDone();
});

// Test that clicking <a href=#> doesn't actually go to #.
TEST_F('BaseDownloadsWebUITest', 'PoundLinkClicksDontChangeUrl', function() {
  assertEquals(this.browsePreload, document.location.href);
  document.querySelector('.clear-all-link').click();
  assertEquals(this.browsePreload, document.location.href);
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
TEST_F('DownloadsWebUIDeleteProhibitedTest', 'DeleteProhibited', function() {
  this.testHelper(false, false);
  // TODO(pamg): Mock out the back-end calls, so we can also test removing a
  // single item.
  testDone();
});

/**
 * Fixture for Downloads WebUI testing for a supervised user.
 * @extends {BaseDownloadsWebUITest}
 * @constructor
 */
function DownloadsWebUIForSupervisedUsersTest() {}

DownloadsWebUIForSupervisedUsersTest.prototype = {
  __proto__: BaseDownloadsWebUITest.prototype,

  /** @override */
  typedefCppFixture: 'DownloadsWebUIForSupervisedUsersTest',
};

// Test UI for supervised users, removing entries should be disabled
// and removal controls should be hidden.
TEST_F('DownloadsWebUIForSupervisedUsersTest', 'SupervisedUsers', function() {
  this.testHelper(false, true);
  testDone();
});
