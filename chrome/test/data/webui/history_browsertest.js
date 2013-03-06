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
    time: timestamp,
    title: d.toString(),  // Use the stringified date as the title.
    url: 'http://google.com/' + timestamp
  };
}

/**
 * Wait for the history backend to call the global function named
 * |callbackName|, and then execute |afterFunction|. This allows tests to
 * wait on asynchronous backend operations before proceeding.
 */
function waitForCallback(callbackName, afterFunction) {
  var originalCallback = window[callbackName];

  // Install a wrapper that temporarily replaces the original function.
  window[callbackName] = function() {
    window[callbackName] = originalCallback;
    originalCallback.apply(this, arguments);
    afterFunction();
  };
}

/**
 * Asynchronously execute the global function named |functionName|. This
 * should be used for all calls from backend stubs to the frontend.
 */
function callFrontendAsync(functionName) {
  var args = Array.prototype.slice.call(arguments, 1);
  setTimeout(function() {
    window[functionName].apply(window, args);
  }, 1);
}

/**
 * Checks that all the checkboxes in the [|start|, |end|] interval are checked
 * and that their IDs are properly set. Does that against the checkboxes in
 * |checked|, starting from the |startInChecked| position.
 * @param {Array} checked An array of all the relevant checked checkboxes
 *    on this page.
 * @param {Number} start The starting checkbox id.
 * @param {Number} end The ending checkbox id.
 */
function checkInterval(checked, start, end) {
  for (var i = start; i <= end; i++)
    expectEquals('checkbox-' + i, checked[i - start].id);
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
    this.registerMockHandler_(
        'queryHistory', this.queryHistoryStub_.bind(this));
  },

  /**
   * Register a mock handler for a message to the history backend.
   * @param handlerName The name of the message to mock.
   * @param handler The mock message handler function.
   */
   registerMockHandler_: function(handlerName, handler) {
    // Mock4JS doesn't pass in the actual arguments to the stub, but it _will_
    // pass the original args to the matcher object. SaveMockArguments acts as
    // a proxy for another matcher, but keeps track of all the arguments it was
    // asked to match.
    var savedArgs = new SaveMockArguments();

    this.makeAndRegisterMockHandler([handlerName]);
    this.mockHandler.stubs()[handlerName](savedArgs.match(ANYTHING)).will(
        callFunctionWithSavedArgs(savedArgs, handler));
  },

  /**
   * Default stub for the queryHistory message to the history backend.
   * Simulates an empty history database. Override this to customize this
   * behavior for particular tests.
   * @param {Array} arguments The original arguments to queryHistory.
   */
  queryHistoryStub_: function(args) {
    callFrontendAsync(
        'historyResult', { term: args[0], finished: true }, []);
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

  preLoad: function() {
    BaseHistoryWebUITest.prototype.preLoad.call(this);

    this.registerMockHandler_(
        'removeUrlsOnOneDay', this.removeUrlsStub_.bind(this));

    // Prepare a list of fake history results. The entries will begin at
    // 1:00 AM on Sept 2, 2008, and will be spaced two minutes apart.
    var timestamp = new Date(2008, 9, 2, 1, 0).getTime();
    this.fakeHistory_ = [];

    for (var i = 0; i < TOTAL_RESULT_COUNT; i++) {
      this.fakeHistory_.push(createHistoryEntry(timestamp));
      timestamp -= 2 * 60 * 1000;  // Next visit is two minutes earlier.
    }
  },

  /**
   * Stub for the 'queryHistory' message to the history backend.
   * Simulates a history database using the fake history data that is
   * initialized in preLoad().
   * @param {Array} arguments The original arguments to queryHistory.
   */
  queryHistoryStub_: function(args) {
    var searchText = args[0];
    var offset = args[1];
    var range = args[2];
    var endTime = args[3] || Number.MAX_VALUE;
    var maxCount = args[4];

    // Advance past all entries newer than the specified end time.
    var i = 0;
    while (this.fakeHistory_[i] && this.fakeHistory_[i].time >= endTime)
      ++i;

    var results = this.fakeHistory_.slice(i);
    if (maxCount)
      results = results.slice(0, maxCount);

    callFrontendAsync(
        'historyResult',
        {
          term: searchText,
          finished: (this.fakeHistory_.length <= i + results.length)
        },
        results);
  },

  /**
   * Stub for the 'removeUrlsOnOneDay' message to the history backend.
   * This will modify the fake history data in the test instance, so that
   * further 'queryHistory' messages will not contain the deleted entries.
   * @param {Array} arguments The original arguments to removeUrlsOnOneDay.
   */
  removeUrlsStub_: function(args) {
    var day = new Date(args[0]).toDateString();
    var urls = args.slice(1);

    // Remove the matching URLs from the fake history data.
    var newHistory = [];
    for (var i = 0, visit; visit = this.fakeHistory_[i]; ++i) {
      if (urls.indexOf(visit.url) < 0 || visit.dateRelativeDay != day)
        newHistory.push(visit);
    }
    this.fakeHistory_ = newHistory;
    callFrontendAsync('deleteComplete');
  }
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
  waitForCallback('historyResult', function() {
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
    var newDayHeaders = document.querySelectorAll('.day');
    expectEquals(2, newDayHeaders.length);

    expectNotEquals(newDayHeaders[0].textContent,
                    newDayHeaders[1].textContent);
    expectEquals(dayHeaders[0].textContent, newDayHeaders[0].textContent);
    expectEquals(dayHeaders[1].textContent, newDayHeaders[1].textContent);

    testDone();
  });
});

/**
 * Test deletion of history entries.
 */
TEST_F('HistoryWebUITest', 'deletion', function() {
  var checkboxes = document.querySelectorAll(
      '#results-display input[type=checkbox]');

  // Confirm all the things!!!
  window.confirm = function() { return true; };

  // The "remote" button should be initially selected.
  var removeButton = $('remove-selected');
  expectTrue(removeButton.disabled);

  checkboxes[0].click();
  expectFalse(removeButton.disabled);

  var firstEntry = document.querySelector('.title a').textContent;
  removeButton.click();

  // After deletion, expect the results to be reloaded.
  waitForCallback('historyResult', function() {
    expectNotEquals(document.querySelector('.title a').textContent, firstEntry);
    expectTrue(removeButton.disabled);

    // Delete the first 3 entries.
    checkboxes = document.querySelectorAll(
        '#results-display input[type=checkbox]');
    checkboxes[0].click();
    checkboxes[1].click();
    checkboxes[2].click();
    expectFalse(removeButton.disabled);

    var nextEntry = document.querySelectorAll('.title a')[3];
    removeButton.click();
    waitForCallback('historyResult', function() {
      // The next entry after the deleted ones should now be the first.
      expectEquals(document.querySelector('.title a').textContent,
                   nextEntry.textContent);
      testDone();
    });
  });
});

/**
 * Test selecting multiple entries using shift click.
 */
TEST_F('HistoryWebUITest', 'multipleSelect', function() {
  var checkboxes = document.querySelectorAll(
      '#results-display input[type=checkbox]');

  var getAllChecked = function () {
    return Array.prototype.slice.call(document.querySelectorAll(
        '#results-display input[type=checkbox]:checked'));
  }

  // Make sure that nothing is checked.
  expectEquals(0, getAllChecked().length);

  var shiftClick = function(el) {
    el.dispatchEvent(new MouseEvent('click', { shiftKey: true }));
  };

  // Check the start.
  shiftClick($('checkbox-4'));
  // And the end.
  shiftClick($('checkbox-9'));

  // See if they are checked.
  var checked = getAllChecked();
  expectEquals(6, checked.length);
  checkInterval(checked, 4, 9);

  // Extend the selection.
  shiftClick($('checkbox-14'));

  checked = getAllChecked();
  expectEquals(11, checked.length);
  checkInterval(checked, 4, 14);

  // Now do a normal click on a higher ID box and a shift click on a lower ID
  // one (test the other way around).
  $('checkbox-24').click();
  shiftClick($('checkbox-19'));

  checked = getAllChecked();
  expectEquals(17, checked.length);
  // First set of checkboxes (11).
  checkInterval(checked, 4, 14);
  // Second set (6).
  checkInterval(checked.slice(11), 19, 24);

  // Test deselection.
  $('checkbox-26').click();
  shiftClick($('checkbox-20'));

  checked = getAllChecked();
  // checkbox-20 to checkbox-24 should be deselected now.
  expectEquals(12, checked.length);
  // First set of checkboxes (11).
  checkInterval(checked, 4, 14);
  // Only checkbox-19 should still be selected.
  expectEquals('checkbox-19', checked[11].id);

  testDone();
});
