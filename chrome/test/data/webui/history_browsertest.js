// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const */ var TOTAL_RESULT_COUNT = 160;

/**
 * TestFixture for History WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function HistoryWebUITest() {}

HistoryWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the history page & call our preLoad().
   */
  browsePreload: 'chrome://history-frame',

  isAsync: true,

  /**
   * Register handlers to stub out calls to the history backend.
   * @override
   */
  preLoad: function() {
    // The number of results to return per call to getHistory/searchHistory.
    var RESULT_SIZE = 20;

    // Prepare a list of fake history results. The entries will begin at
    // 1:00 AM on Sept 2, 2008, and will be spaced two minutes apart.
    var timestamp = new Date(2008, 9, 2, 1, 0).getTime();
    var history = [];
    for (var i = 0; i < TOTAL_RESULT_COUNT; i++) {
      history.push(this.createHistoryEntry_(timestamp));
      timestamp -= 2 * 60 * 1000;  // Next visit is two minutes earlier.
    }

    // Stub out the calls to get history results from the backend.

    this.makeAndRegisterMockHandler(['getHistory', 'searchHistory']);
    this.mockHandler.stubs().getHistory(NOT_UNDEFINED).will(callFunction(
      function(depthInDays) {
        var results = history.slice(0, RESULT_SIZE);
        history = history.slice(results.length);
        historyResult({ term: '', finished: history.length == 0 }, results);
      }));
  },

  /**
   * Create a fake history result with the given timestamp.
   * @param {Number} timestamp Timestamp of the entry, in ms since the epoch.
   * @return {Object} An object representing a history entry.
   * @private
   */
  createHistoryEntry_: function(timestamp) {
    var d = new Date(timestamp);
    return {
      dateTimeOfDay: d.getHours() + ':' + d.getMinutes(),
      dateRelativeDay: d.toDateString(),
      starred: false,
      time: timestamp / 1000,  // History front-end expects timestamp in secs.
      title: d.toString(),  // Use the stringified date as the title.
      url: 'http://google.com/' + timestamp
    };
  },
};

TEST_F('HistoryWebUITest', 'basicTest', function() {
  var resultCount = document.querySelectorAll('.entry').length;

  // Check that there are two days of entries.
  var dayHeaders = document.querySelectorAll('.day');
  expectEquals(2, dayHeaders.length);
  expectNotEquals(dayHeaders[0].textContent, dayHeaders[1].textContent);

  // Check that the entries in each day are time-ordered, and that no
  // duplicate URLs appear on a given day.
  var urlsByDay = {};
  var lastDate = new Date();
  for (var day = 0; day < dayHeaders.length; ++day) {
    var dayTitle = dayHeaders[day].textContent;
    var dayResults = document.querySelectorAll('.day-results')[day];
    var entries = dayResults.querySelectorAll('.entry');
    expectGT(entries.length, 0);

    for (var i = 0, entry; entry = entries[i]; ++i) {
      var time = entry.querySelector('.time').textContent;
      expectGT(time.length, 0);

      var date = new Date(dayTitle + ' ' + time);
      expectGT(lastDate, date);
      lastDate = date;

      // Ensure it's not a duplicate URL for this day.
      var dayAndUrl = day + entry.querySelector('a').href;
      expectFalse(urlsByDay.hasOwnProperty(dayAndUrl));
      urlsByDay[dayAndUrl] = dayAndUrl;

      // Reconstruct the entry date from the title, and ensure that it's
      // consistent with the date header and with the time.
      var entryDate = new Date(entry.querySelector('.title').textContent);
      expectEquals(entryDate.getYear(), date.getYear());
      expectEquals(entryDate.getMonth(), date.getMonth());
      expectEquals(entryDate.getDay(), date.getDay());
      expectEquals(entryDate.getHours(), date.getHours());
      expectEquals(entryDate.getMinutes(), date.getMinutes());
    }
  }

  // Ensure that there is a link to the next page of results.
  var linkButtons = document.querySelectorAll('.link-button');
  assertEquals(1, linkButtons.length)

  // Go to the next page.
  linkButtons[0].click();
  resultCount += document.querySelectorAll('.entry').length;

  // Check that the two pages include all of the entries.
  expectEquals(TOTAL_RESULT_COUNT, resultCount);

  // There should now be two links: "newest", and "newer".
  linkButtons = document.querySelectorAll('.link-button');
  assertEquals(2, linkButtons.length);

  // Go back to the first page, and check that the same day headers are there.
  linkButtons[0].click();
  var newDayHeaders = document.querySelectorAll('.day');
  expectEquals(2, newDayHeaders.length);
  expectNotEquals(newDayHeaders[0].textContent, newDayHeaders[1].textContent);
  expectEquals(dayHeaders[0].textContent, newDayHeaders[0].textContent);
  expectEquals(dayHeaders[1].textContent, newDayHeaders[1].textContent);

  testDone();
});
