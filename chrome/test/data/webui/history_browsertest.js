// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const */ var TOTAL_RESULT_COUNT = 160;
/** @const */ var WAIT_TIMEOUT = 200;

/**
 * Create a fake history result with the given timestamp.
 * @param {Number} timestamp Timestamp of the entry, in ms since the epoch.
 * @return {Object} An object representing a history entry.
 */
function createHistoryEntry(timestamp) {
  var d = new Date(timestamp);
  return {
    dateTimeOfDay: d.getHours() + ':' + d.getMinutes(),
    dateRelativeDay: d.toDateString(),
    starred: false,
    time: timestamp / 1000,  // History front-end expects timestamp in secs.
    title: d.toString(),  // Use the stringified date as the title.
    url: 'http://google.com/' + timestamp
  };
}

/**
 * Base fixture for History WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function BaseHistoryWebUITest() {}

BaseHistoryWebUITest.prototype = {
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
    // Mock4JS doesn't pass in the actual arguments to the stub, but it _will_
    // pass the original args to the matcher object. SaveMockArguments acts as
    // a proxy for another matcher, but keeps track of all the arguments it was
    // asked to match.
    var savedArgs = new SaveMockArguments();

    this.makeAndRegisterMockHandler(['queryHistory']);
    this.mockHandler.stubs().queryHistory(savedArgs.match(ANYTHING)).will(
        callFunctionWithSavedArgs(savedArgs, this.queryHistoryStub_));
  },

  /**
   * Default stub for the queryHistory message to the history backend.
   * Simulates an empty history database. Override this to customize this
   * behavior for particular tests.
   * @param {Array} arguments The original arguments to queryHistory.
   */
  queryHistoryStub_: function(args) {
    // Respond asynchronously to simulate real behavior in Chrome.
    setTimeout(function() {
      historyResult({ term: args[0], finished: true, cursor: 0 }, []);
    }, 1);
  },
};

/**
 * Fixture for History WebUI testing which returns some fake history results
 * to the frontend.
 * @extends {BaseHistoryWebUITest}
 * @constructor
 */
function HistoryWebUITest() {}

HistoryWebUITest.prototype = {
  __proto__: BaseHistoryWebUITest.prototype,

  queryHistoryStub_: (function() {
    // Prepare a list of fake history results. The entries will begin at
    // 1:00 AM on Sept 2, 2008, and will be spaced two minutes apart.
    var timestamp = new Date(2008, 9, 2, 1, 0).getTime();

    return function (args) {
      var searchText = args[0];
      var endTime = args[1];
      var cursor = args[2];
      var maxCount = args[3];

      // Generate the requested number of results.
      var results = [];
      var resultCount = Math.min(maxCount, TOTAL_RESULT_COUNT - cursor);
      for (var i = 0; i < resultCount; i++) {
        results.push(createHistoryEntry(timestamp));
        timestamp -= 2 * 60 * 1000;  // Next visit is two minutes earlier.
      }

      // Respond asynchronously to simulate real behavior in Chrome.
      setTimeout(function() {
        historyResult(
            {
              term: searchText,
              finished: cursor + results.length >= TOTAL_RESULT_COUNT,
              cursor: cursor + results.length,
            },
            results);
      }, 1);
    }
  }()),
};

TEST_F('BaseHistoryWebUITest', 'emptyHistory', function() {
  expectTrue($('newest-button').hidden);
  expectTrue($('newer-button').hidden);
  expectTrue($('older-button').hidden);
  testDone();
});

TEST_F('HistoryWebUITest', 'basicTest', function() {
  var resultCount = document.querySelectorAll('.entry').length;

  // Check that there are two days of entries.
  var dayHeaders = document.querySelectorAll('.day');
  assertEquals(2, dayHeaders.length);
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

  // Check that there are 3 page navigation links and that only the "Older"
  // link is visible.
  expectEquals(3, document.querySelectorAll('.link-button').length)
  expectTrue($('newest-button').hidden);
  expectTrue($('newer-button').hidden);
  expectFalse($('older-button').hidden);

  // Go to the next page.
  $('older-button').click();
  setTimeout(function() {
    resultCount += document.querySelectorAll('.entry').length;

    // Check that the two pages include all of the entries.
    expectEquals(TOTAL_RESULT_COUNT, resultCount);

    // Check that the day header was properly continued -- the header for the
    // last day on the first page should be a substring of the header on the
    // second page. E.g. "Wed, Oct 8, 2008" and "Web, Oct 8, 2008 - cont'd".
    var newDayHeaders = document.querySelectorAll('.day');
    expectEquals(1, newDayHeaders.length);
    expectEquals(0,
        newDayHeaders[0].textContent.indexOf(dayHeaders[1].textContent));

    // Check that the "Newest" and "Newer" links are now visible, but the
    // "Older" link is hidden.
    expectEquals(3, document.querySelectorAll('.link-button').length)
    expectFalse($('newest-button').hidden);
    expectFalse($('newer-button').hidden);
    expectTrue($('older-button').hidden);

    // Go back to the first page, and check that the same day headers are there.
    $('newest-button').click();
    setTimeout(function() {
      var newDayHeaders = document.querySelectorAll('.day');
      expectEquals(2, newDayHeaders.length);

      expectNotEquals(newDayHeaders[0].textContent,
                      newDayHeaders[1].textContent);
      expectEquals(dayHeaders[0].textContent, newDayHeaders[0].textContent);
      expectEquals(dayHeaders[1].textContent, newDayHeaders[1].textContent);

      testDone();
    }, WAIT_TIMEOUT);
  }, WAIT_TIMEOUT);
});
