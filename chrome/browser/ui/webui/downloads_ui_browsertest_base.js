// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    var list = [];
    for (var i = 0; i < TOTAL_RESULT_COUNT; ++i) {
      list.push(this.createDownload(i, timestamp));
      timestamp += 2 * 60 * 1000;  // Next visit is two minutes later.
    }
    downloads.Manager.updateAll(list);
    expectEquals(downloads.Manager.size(), TOTAL_RESULT_COUNT);
  },

  /**
   * Creates a download object to be passed to the page, following the expected
   * backend format (see downloads_dom_handler.cc).
   * @param {number} A unique ID for the download.
   * @param {number} The time the download purportedly started.
   * @return {!Object} A fake download object.
   */
  createDownload: function(id, timestamp) {
    return {
      id: id,
      started: timestamp,
      otr: false,
      state: downloads.Item.States.COMPLETE,
      retry: false,
      file_path: '/path/to/file',
      file_url: 'http://google.com/' + timestamp,
      file_name: 'download_' + timestamp,
      url: 'http://google.com/' + timestamp,
      file_externally_removed: false,
      danger_type: downloads.Item.DangerType.NOT_DANGEROUS,
      last_reason_text: '',
      since_string: 'today',
      date_string: 'today',
      percent: 100,
      progress_status_text: 'done',
      received: 128,
    };
  },

  /**
   * Simulates getting no results from C++.
   */
  sendEmptyList: function() {
    downloads.Manager.updateAll([]);
    assertEquals(0, downloads.Manager.size());
  },

  /**
   * Check that |element| is showing and contains |text|.
   * @param {Element} element
   * @param {string} text
   */
  checkShowing: function(element, text) {
    expectFalse(element.hidden);
    expectNotEquals(-1, element.textContent.indexOf(text));
  },

  /**
   * Asserts the correctness of the state of the UI elements that delete the
   * download history.
   * @param {boolean} visible True if download deletion UI should be visible.
   */
  expectDeleteControlsVisible: function(visible) {
    // "Clear all" should only be showing when deletions are allowed.
    expectEquals(!visible, $('clear-all').hidden);

    // "Remove from list" links should only exist when deletions are allowed.
    var query = '#downloads-display .safe .remove';
    if (!visible)
      query += '[hidden]';
    expectEquals(TOTAL_RESULT_COUNT, document.querySelectorAll(query).length);
  },
};
