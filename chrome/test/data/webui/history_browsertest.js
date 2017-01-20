// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "chrome/test/data/webui/history_ui_browsertest.h"');

/** @const */ var TOTAL_RESULT_COUNT = 160;
/** @const */ var WAIT_TIMEOUT = 200;

/**
 * Test fixture for history WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function HistoryUIBrowserTest() {}

/**
 * Create a fake history result with the given timestamp.
 * @param {Number} timestamp Timestamp of the entry, in ms since the epoch.
 * @param {String} url The URL to set on this entry.
 * @return {Object} An object representing a history entry.
 */
function createHistoryEntry(timestamp, url) {
  var d = new Date(timestamp);
  // Extract domain from url.
  var domainMatch = url.replace(/^.+?:\/\//, '').match(/[^/]+/);
  var domain = domainMatch ? domainMatch[0] : '';
  return {
    dateTimeOfDay: d.getHours() + ':' + d.getMinutes(),
    dateRelativeDay: d.toDateString(),
    allTimestamps: [timestamp],
    starred: false,
    time: timestamp,
    title: d.toString(),  // Use the stringified date as the title.
    url: url,
    domain: domain
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
 * Returns a period of 7 days, |offset| weeks back from |today|. The behavior
 * of this function should be identical to
 * BrowsingHistoryHandler::SetQueryTimeInWeeks.
 * @param {Number} offset Number of weeks to go back.
 * @param {Date} today Which date to consider as "today" (since we're not using
 *     the actual current date in this case).
 * @return {Object} An object containing the begin date and the end date of the
 *     computed period.
 */
function setQueryTimeInWeeks(offset, today) {
  // Going back one day at a time starting from midnight will make sure that
  // the other values get updated properly.
  var endTime = new Date(today);
  endTime.setHours(24, 0, 0, 0);
  for (var i = 0; i < 7 * offset; i++)
    endTime.setDate(endTime.getDate() - 1);
  var beginTime = new Date(endTime);
  for (var i = 0; i < 7; i++)
    beginTime.setDate(beginTime.getDate() - 1);
  return {'endTime': endTime, 'beginTime': beginTime};
}

/**
 * Returns the period of a month, |offset| months back from |today|. The
 * behavior of this function should be identical to
 * BrowsingHistoryHandler::SetQueryTimeInMonths.
 * @param {Number} offset Number of months to go back.
 * @param {Date} today Which date to consider as "today" (since we're not using
 *     the actual current date in this case).
 * @return {Object} An object containing the begin date and the end date of the
 *     computed period.
 */
function setQueryTimeInMonths(offset, today) {
  var endTime = new Date(today);
  var beginTime = new Date(today);
  // Last day of this month.
  endTime.setMonth(endTime.getMonth() + 1, 0);
  // First day of the current month.
  beginTime.setMonth(beginTime.getMonth(), 1);
  for (var i = 0; i < offset; i++) {
    beginTime.setMonth(beginTime.getMonth() - 1);
    endTime.setMonth(endTime.getMonth() - 1);
  }
  return {'endTime': endTime, 'beginTime': beginTime};
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

  /** @override */
  typedefCppFixture: 'HistoryUIBrowserTest',

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /** @override */
  isAsync: true,
};

/**
 * Fixture for History WebUI testing which returns some fake history results
 * to the frontend. Other fixtures that want to stub out calls to the backend
 * can extend this one.
 * @extends {BaseHistoryWebUITest}
 * @constructor
 */
function HistoryWebUIFakeBackendTest() {
}

HistoryWebUIFakeBackendTest.prototype = {
  __proto__: BaseHistoryWebUITest.prototype,

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
  }
};

function queryHistoryImpl(args, beginTime, history) {
  var searchText = args[0];
  var offset = args[1];
  var range = args[2];
  var endTime = args[3] || Number.MAX_VALUE;
  var maxCount = args[4];

  var results = [];
  if (searchText) {
    for (var k = 0; k < history.length; k++) {
      // Search only by title in this stub.
      if (history[k].title.indexOf(searchText) != -1)
        results.push(history[k]);
    }
  } else {
    results = history;
  }

  // Advance past all entries newer than the specified end time.
  var i = 0;
  // Finished is set from the history database so this behavior may not be
  // completely identical.
  var finished = true;
  while (i < results.length && results[i].time >= endTime)
    ++i;

  if (beginTime) {
    var j = i;
    while (j < results.length && results[j].time >= beginTime)
      ++j;

    finished = (j == results.length);
    results = results.slice(i, j);
  } else {
    results = results.slice(i);
  }

  if (maxCount) {
    finished = (maxCount >= results.length);
    results = results.slice(0, maxCount);
  }

  var queryStartTime = '';
  var queryEndTime = '';
  if (results.length) {
    queryStartTime = results[results.length - 1].dateRelativeDay;
    queryEndTime = results[0].dateRelativeDay;
  } else if (beginTime) {
    queryStartTime = Date(beginTime);
    queryEndTime = Date(endTime);
  }

  callFrontendAsync(
      'historyResult',
      {
        term: searchText,
        finished: finished,
        queryInterval: queryStartTime + ' - ' + queryEndTime,
      },
      results);
}

/**
 * Fixture for History WebUI testing which returns some fake history results
 * to the frontend.
 * @extends {HistoryWebUIFakeBackendTest}
 * @constructor
 */
function HistoryWebUITest() {}

HistoryWebUITest.prototype = {
  __proto__: HistoryWebUIFakeBackendTest.prototype,

  preLoad: function() {
    HistoryWebUIFakeBackendTest.prototype.preLoad.call(this);

    this.registerMockHandler_(
        'removeVisits', this.removeVisitsStub_.bind(this));

    // Prepare a list of fake history results. The entries will begin at
    // 1:00 AM on Sept 2, 2008, and will be spaced two minutes apart.
    var timestamp = new Date(2008, 9, 2, 1, 0).getTime();
    this.fakeHistory_ = [];

    for (var i = 0; i < TOTAL_RESULT_COUNT; i++) {
      this.fakeHistory_.push(
          createHistoryEntry(timestamp, 'http://google.com/' + timestamp));
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
    if (range == HistoryModel.Range.ALL_TIME) {
      queryHistoryImpl(args, null, this.fakeHistory_);
      return;
    }
    if (range == HistoryModel.Range.WEEK)
      var interval = setQueryTimeInWeeks(offset, this.today);
    else
      var interval = setQueryTimeInMonths(offset, this.today);

    args[3] = interval.endTime.getTime();
    queryHistoryImpl(args, interval.beginTime.getTime(), this.fakeHistory_);
  },

  /**
   * Stub for the 'removeVisits' message to the history backend.
   * This will modify the fake history data in the test instance, so that
   * further 'queryHistory' messages will not contain the deleted entries.
   * @param {Array} arguments The original arguments to removeVisits.
   */
  removeVisitsStub_: function(args) {
    for (var i = 0; i < args.length; ++i) {
      var url = args[i].url;
      var timestamps = args[i].timestamps;
      assertEquals(timestamps.length, 1);
      this.removeVisitsToUrl_(url, new Date(timestamps[0]));
    }
    callFrontendAsync('deleteComplete');
  },

  /**
   * Removes any visits to |url| on the same day as |date| from the fake
   * history data.
   * @param {string} url
   * @param {Date} date
   */
  removeVisitsToUrl_: function(url, date) {
    var day = date.toDateString();
    var newHistory = [];
    for (var i = 0, visit; visit = this.fakeHistory_[i]; ++i) {
      if (url != visit.url || visit.dateRelativeDay != day)
        newHistory.push(visit);
    }
    this.fakeHistory_ = newHistory;
  }
};

/**
 * Examines the time column of every entry on the page, and ensure that they
 * are all the same width.
 */
function ensureTimeWidthsEqual() {
  var times = document.querySelectorAll('.entry .time');
  var timeWidth = times[0].clientWidth;
  for (var i = 1; i < times.length; ++i) {
    assertEquals(timeWidth, times[i].clientWidth);
  }
}

// Times out on Mac: http://crbug.com/336845
TEST_F('HistoryWebUIFakeBackendTest', 'DISABLED_emptyHistory', function() {
  expectTrue($('newest-button').hidden);
  expectTrue($('newer-button').hidden);
  expectTrue($('older-button').hidden);
  testDone();
});

// Times out on Win: http://crbug.com/336845
TEST_F('HistoryWebUITest', 'DISABLED_basicTest', function() {
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
  expectEquals(3, document.querySelectorAll('[is="action-link"]').length);
  expectTrue($('newest-button').hidden);
  expectTrue($('newer-button').hidden);
  expectFalse($('older-button').hidden);

  ensureTimeWidthsEqual();

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
    expectEquals(3, document.querySelectorAll('[is="action-link"]').length);
    expectFalse($('newest-button').hidden);
    expectFalse($('newer-button').hidden);
    expectTrue($('older-button').hidden);

    ensureTimeWidthsEqual();

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
 * Test bulk deletion of history entries.
 * Disabled because it is currently very flaky on the Windows XP bot.
 */
TEST_F('HistoryWebUITest', 'DISABLED_bulkDeletion', function() {
  var checkboxes = document.querySelectorAll(
      '#results-display input[type=checkbox]');

  // Immediately confirm the history deletion.
  confirmDeletion = function(okCallback, cancelCallback) {
    okCallback();
  };

  // The "remove" button should be initially disabled.
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
 * Disabled due to time out on all platforms: crbug/375910
 */
TEST_F('HistoryWebUITest', 'DISABLED_multipleSelect', function() {
  var checkboxes = document.querySelectorAll(
      '#results-display input[type=checkbox]');

  var getAllChecked = function() {
    return Array.prototype.slice.call(document.querySelectorAll(
        '#results-display input[type=checkbox]:checked'));
  };

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

TEST_F('HistoryWebUITest', 'DISABLED_searchHistory', function() {
  var getResultCount = function() {
    return document.querySelectorAll('.entry').length;
  };
  // See that all the elements are there.
  expectEquals(RESULTS_PER_PAGE, getResultCount());

  // See that the search works.
  $('search-field').value = 'Thu Oct 02 2008';
  $('search-button').click();

  waitForCallback('historyResult', function() {
    expectEquals(31, getResultCount());

    // Clear the search.
    $('search-field').value = '';
    $('search-button').click();
    waitForCallback('historyResult', function() {
      expectEquals(RESULTS_PER_PAGE, getResultCount());
      testDone();
    });
  });
});

function setPageState(searchText, page, groupByDomain, range, offset) {
  window.location = '#' + PageState.getHashString(
      searchText, page, groupByDomain, range, offset);
}

function RangeHistoryWebUITest() {}

RangeHistoryWebUITest.prototype = {
  __proto__: HistoryWebUITest.prototype,

  /** @override */
  preLoad: function() {
    HistoryWebUITest.prototype.preLoad.call(this);
    // Repeat the domain visits every 4 days. The nested lists contain the
    // domain suffixes for the visits in a day.
    var domainSuffixByDay = [
      [1, 2, 3, 4],
      [1, 2, 2, 3],
      [1, 2, 1, 2],
      [1, 1, 1, 1]
    ];

    var buildDomainUrl = function(timestamp) {
      var d = new Date(timestamp);
      // Repeat the same setup of domains every 4 days.
      var day = d.getDate() % 4;
      // Assign an entry for every 6 hours so that we get 4 entries per day
      // maximum.
      var visitInDay = Math.floor(d.getHours() / 6);
      return 'http://google' + domainSuffixByDay[day][visitInDay] + '.com/' +
          timestamp;
    };

    // Prepare a list of fake history results. Start the results on
    // 11:00 PM on May 2, 2012 and add 4 results every day (one result every 6
    // hours).
    var timestamp = new Date(2012, 4, 2, 23, 0).getTime();
    this.today = new Date(2012, 4, 2);
    this.fakeHistory_ = [];

    // Put in 2 days for May and 30 days for April so the results span over
    // the month limit.
    for (var i = 0; i < 4 * 32; i++) {
      this.fakeHistory_.push(
          createHistoryEntry(timestamp, buildDomainUrl(timestamp)));
      timestamp -= 6 * 60 * 60 * 1000;
    }

    // Leave March empty.
    timestamp -= 31 * 24 * 3600 * 1000;

    // Put results in February.
    for (var i = 0; i < 29 * 4; i++) {
      this.fakeHistory_.push(
          createHistoryEntry(timestamp, buildDomainUrl(timestamp)));
      timestamp -= 6 * 60 * 60 * 1000;
    }
  },

  setUp: function() {
    // Show the filter controls as if the command line switch was active.
    $('top-container').hidden = true;
    $('history-page').classList.add('big-topbar-page');
    $('filter-controls').hidden = false;
    expectFalse($('filter-controls').hidden);
  },
};

/**
 * Disabled due intermitent failures on multiple OSes http://crbug.com/377338
 */
TEST_F('RangeHistoryWebUITest', 'DISABLED_allView', function() {
  // Check that we start off in the all time view.
  expectTrue($('timeframe-controls').querySelector('input').checked);
  // See if the correct number of days is shown.
  var dayHeaders = document.querySelectorAll('.day');
  assertEquals(Math.ceil(RESULTS_PER_PAGE / 4), dayHeaders.length);
  testDone();
});

/**
 * Checks whether the domains in a day are ordered decreasingly.
 * @param {Element} element Ordered list containing the grouped domains for a
 *     day.
 */
function checkGroupedVisits(element) {
  // The history page contains the number of visits next to a domain in
  // parentheses (e.g. 'google.com (5)'). This function extracts that number
  // and returns it.
  var getNumberVisits = function(element) {
    return parseInt(element.textContent.replace(/\D/g, ''), 10);
  };

  // Read the number of visits from each domain and make sure that it is lower
  // than or equal to the number of visits from the previous domain.
  var domainEntries = element.querySelectorAll('.number-visits');
  var currentNumberOfVisits = getNumberVisits(domainEntries[0]);
  for (var j = 1; j < domainEntries.length; j++) {
    var numberOfVisits = getNumberVisits(domainEntries[j]);
    assertTrue(currentNumberOfVisits >= numberOfVisits);
    currentNumberOfVisits = numberOfVisits;
  }
}

// Times out on Mac and Win: http://crbug.com/336845
TEST_F('RangeHistoryWebUITest', 'DISABLED_weekViewGrouped', function() {
  // Change to weekly view.
  setPageState('', 0, HistoryModel.Range.WEEK, 0);
  waitForCallback('historyResult', function() {
    // See if the correct number of days is still shown.
    var dayResults = document.querySelectorAll('.day-results');
    assertEquals(7, dayResults.length);

    // Check whether the results are ordered by visits.
    for (var i = 0; i < dayResults.length; i++)
      checkGroupedVisits(dayResults[i]);

    ensureTimeWidthsEqual();

    testDone();
  });
});

// Times out on Mac and Win: http://crbug.com/336845
TEST_F('RangeHistoryWebUITest', 'DISABLED_monthViewGrouped', function() {
  // Change to monthly view.
  setPageState('', 0, HistoryModel.Range.MONTH, 0);
  waitForCallback('historyResult', function() {
    // See if the correct number of days is shown.
    var monthResults = document.querySelectorAll('.month-results');
    assertEquals(1, monthResults.length);

    checkGroupedVisits(monthResults[0]);
    ensureTimeWidthsEqual();

    testDone();
  });
});

// Times out on Mac: http://crbug.com/336845
TEST_F('RangeHistoryWebUITest', 'DISABLED_monthViewEmptyMonth', function() {
  // Change to monthly view.
  setPageState('', 0, HistoryModel.Range.MONTH, 2);

  waitForCallback('historyResult', function() {
    // See if the correct number of days is shown.
    var resultsDisplay = $('results-display');
    assertEquals(0, resultsDisplay.querySelectorAll('.months-results').length);
    var noResults = loadTimeData.getString('noResults');
    assertNotEquals(-1, $('results-header').textContent.indexOf(noResults));

    testDone();
  });
});

/**
 * Fixture for History WebUI tests using the real history backend.
 * @extends {BaseHistoryWebUITest}
 * @constructor
 */
function HistoryWebUIRealBackendTest() {}

HistoryWebUIRealBackendTest.prototype = {
  __proto__: BaseHistoryWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    // Add some visits to the history database.
    GEN('  AddPageToHistory(0, "http://google.com", "Google");');
    GEN('  AddPageToHistory(1, "http://example.com", "Example");');
    GEN('  AddPageToHistory(2, "http://google.com", "Google");');

    // Add a visit on the next day.
    GEN('  AddPageToHistory(36, "http://google.com", "Google");');
  },

  /** @override */
  setUp: function() {
    BaseHistoryWebUITest.prototype.setUp.call(this);

    // Enable when failure is resolved.
    // AX_TEXT_04: http://crbug.com/560914
    this.accessibilityAuditConfig.ignoreSelectors(
        'linkWithUnclearPurpose',
        '#notification-bar > SPAN > A');
  },
};

/**
 * Simple test that verifies that the correct entries are retrieved from the
 * history database and displayed in the UI.
 */
// Times out on Mac and Win: http://crbug.com/336845
TEST_F('HistoryWebUIRealBackendTest', 'DISABLED_basic', function() {
  // Check that there are two days of entries, and three entries in total.
  assertEquals(2, document.querySelectorAll('.day').length);
  assertEquals(3, document.querySelectorAll('.entry').length);

  testDone();
});

// Times out on Mac: http://crbug.com/336845
TEST_F('HistoryWebUIRealBackendTest',
    'DISABLED_atLeastOneFocusable', function() {
  var results = document.querySelectorAll('#results-display [tabindex="0"]');
  expectGE(results.length, 1);
  testDone();
});

// Times out on Mac: http://crbug.com/336845
TEST_F('HistoryWebUIRealBackendTest',
    'DISABLED_deleteRemovesEntry', function() {
  assertTrue(historyModel.deletingHistoryAllowed);

  var visit = document.querySelector('.entry').visit;
  visit.titleLink.focus();
  assertEquals(visit.titleLink, document.activeElement);

  var deleteKey = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'Delete'});

  assertFalse(historyModel.isDeletingVisits());
  expectFalse(visit.titleLink.dispatchEvent(deleteKey));
  expectTrue(historyModel.isDeletingVisits());

  expectNotEquals(visit.dropDown, document.activeElement);
  testDone();
});

/**
 * Test individual deletion of history entries.
 */
TEST_F('HistoryWebUIRealBackendTest', 'singleDeletion', function() {
  // Deletes the history entry represented by |entryElement|, and calls callback
  // when the deletion is complete.
  var removeEntry = function(entryElement, callback) {
    var dropDownButton = entryElement.querySelector('.drop-down');
    var removeMenuItem = $('remove-visit');

    assertFalse(dropDownButton.disabled);
    assertFalse(removeMenuItem.disabled);

    waitForCallback('onEntryRemoved', callback);

    cr.dispatchSimpleEvent(dropDownButton, 'mousedown');

    var e = new Event('command', {bubbles: true});
    e.command = removeMenuItem.command;
    removeMenuItem.dispatchEvent(e);
  };

  var secondTitle = document.querySelectorAll('.entry a')[1].textContent;
  var thirdTitle = document.querySelectorAll('.entry a')[2].textContent;

  // historyDeleted() should not be called when deleting individual entries
  // using the drop down.
  waitForCallback('historyDeleted', function() {
    testDone([false, 'historyDeleted() called when deleting single entry']);
  });

  expectEquals(2, document.querySelectorAll('.day').length);

  // Delete the first entry. The previous second entry should now be the first.
  removeEntry(document.querySelector('.entry'), function() {
    expectEquals(secondTitle, document.querySelector('.entry a').textContent);

    // After removing the first entry, its day header should also be gone.
    expectEquals(1, document.querySelectorAll('.day').length);

    // Delete another entry. The original third entry should now be the first.
    removeEntry(document.querySelector('.entry'), function() {
      expectEquals(thirdTitle, document.querySelector('.entry a').textContent);
      testDone();
    });
  });
});

TEST_F('HistoryWebUIRealBackendTest', 'leftRightChangeFocus', function() {
  var visit = document.querySelector('.entry').visit;
  visit.titleLink.focus();
  assertEquals(visit.titleLink, document.activeElement);

  var right = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'ArrowRight'});
  expectFalse(visit.titleLink.dispatchEvent(right));

  assertEquals(visit.dropDown, document.activeElement);

  var left = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'ArrowLeft'});
  expectFalse(visit.dropDown.dispatchEvent(left));

  expectEquals(visit.titleLink, document.activeElement);
  testDone();
});

TEST_F('HistoryWebUIRealBackendTest', 'showConfirmDialogAndCancel', function() {
  waitForCallback('deleteComplete', function() {
    testDone([false, "history deleted when it shouldn't have been"]);
  });

  document.querySelector('input[type=checkbox]').click();
  $('remove-selected').click();

  assertTrue($('alertOverlay').classList.contains('showing'));
  assertFalse($('history-page').contains(document.activeElement));

  var esc = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'Escape'});

  document.documentElement.dispatchEvent(esc);
  assertFalse($('alertOverlay').classList.contains('showing'));

  testDone();
});

TEST_F('HistoryWebUIRealBackendTest', 'showConfirmDialogAndRemove', function() {
  document.querySelector('input[type=checkbox]').click();
  $('remove-selected').click();

  assertTrue($('alertOverlay').classList.contains('showing'));
  assertFalse($('history-page').contains(document.activeElement));

  waitForCallback('deleteComplete', testDone);

  var enter = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'Enter'});
  document.documentElement.dispatchEvent(enter);
  assertFalse($('alertOverlay').classList.contains('showing'));
});

// Times out on Mac: http://crbug.com/336845
TEST_F('HistoryWebUIRealBackendTest',
    'DISABLED_menuButtonActivatesOneRow', function() {
  var entries = document.querySelectorAll('.entry');
  assertEquals(3, entries.length);
  assertTrue(entries[0].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));
  assertTrue($('action-menu').hidden);

  // Show the menu via mousedown on the menu button.
  var menuButton = entries[2].querySelector('.menu-button');
  menuButton.dispatchEvent(new MouseEvent('mousedown'));
  expectFalse($('action-menu').hidden);

  // Check that the active item has changed.
  expectTrue(entries[2].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));
  expectFalse(entries[0].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));

  testDone();
});

// Flaky: http://crbug.com/527434
TEST_F('HistoryWebUIRealBackendTest',
    'DISABLED_shiftClickActivatesOneRow', function () {
  var entries = document.querySelectorAll('.entry');
  assertEquals(3, entries.length);
  assertTrue(entries[0].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));

  entries[0].visit.checkBox.focus();
  assertEquals(entries[0].visit.checkBox, document.activeElement);

  entries[0].visit.checkBox.click();
  assertTrue(entries[0].visit.checkBox.checked);

  var entryBox = entries[2].querySelector('.entry-box');
  entryBox.dispatchEvent(new MouseEvent('click', {shiftKey: true}));
  assertTrue(entries[1].visit.checkBox.checked);

  // Focus shouldn't have changed, but the checkbox should toggle.
  expectEquals(entries[0].visit.checkBox, document.activeElement);

  expectTrue(entries[0].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));
  expectFalse(entries[2].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));

  var shiftDown = new MouseEvent('mousedown', {shiftKey: true, bubbles: true});
  entries[2].visit.checkBox.dispatchEvent(shiftDown);
  expectEquals(entries[2].visit.checkBox, document.activeElement);

  // 'focusin' events aren't dispatched while tests are run in batch (e.g.
  // --test-launcher-jobs=2). Simulate this. TODO(dbeam): fix instead.
  cr.dispatchSimpleEvent(document.activeElement, 'focusin', true, true);

  expectFalse(entries[0].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));
  expectTrue(entries[2].classList.contains(cr.ui.FocusRow.ACTIVE_CLASS));

  testDone();
});

/**
 * Fixture for History WebUI testing when deletions are prohibited.
 * @extends {HistoryWebUIRealBackendTest}
 * @constructor
 */
function HistoryWebUIDeleteProhibitedTest() {}

HistoryWebUIDeleteProhibitedTest.prototype = {
  __proto__: HistoryWebUIRealBackendTest.prototype,

  /** @override */
  testGenPreamble: function() {
    HistoryWebUIRealBackendTest.prototype.testGenPreamble.call(this);
    GEN('  SetDeleteAllowed(false);');
  },
};

// Test UI when removing entries is prohibited.
// Times out on Mac: http://crbug.com/336845
TEST_F('HistoryWebUIDeleteProhibitedTest',
    'DISABLED_deleteProhibited', function() {
  // No checkboxes should be created.
  var checkboxes = document.querySelectorAll(
      '#results-display input[type=checkbox]');
  expectEquals(0, checkboxes.length);

  // The "remove" button should be disabled.
  var removeButton = $('remove-selected');
  expectTrue(removeButton.disabled);

  // The "Remove from history" drop-down item should be disabled.
  var removeVisit = $('remove-visit');
  expectTrue(removeVisit.disabled);

  testDone();
});

TEST_F('HistoryWebUIDeleteProhibitedTest', 'atLeastOneFocusable', function() {
  var results = document.querySelectorAll('#results-display [tabindex="0"]');
  expectGE(results.length, 1);
  testDone();
});

TEST_F('HistoryWebUIDeleteProhibitedTest', 'leftRightChangeFocus', function() {
  var visit = document.querySelector('.entry').visit;
  visit.titleLink.focus();
  assertEquals(visit.titleLink, document.activeElement);

  var right = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'ArrowRight'});
  expectFalse(visit.titleLink.dispatchEvent(right));

  assertEquals(visit.dropDown, document.activeElement);

  var left = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'ArrowLeft'});
  expectFalse(visit.dropDown.dispatchEvent(left));

  expectEquals(visit.titleLink, document.activeElement);
  testDone();
});

TEST_F('HistoryWebUIDeleteProhibitedTest', 'deleteIgnored', function() {
  assertFalse(historyModel.deletingHistoryAllowed);

  var visit = document.querySelector('.entry').visit;
  visit.titleLink.focus();
  assertEquals(visit.titleLink, document.activeElement);

  var deleteKey = new KeyboardEvent('keydown',
    {bubbles: true, cancelable: true, key: 'Delete'});

  assertFalse(historyModel.isDeletingVisits());
  expectTrue(visit.titleLink.dispatchEvent(deleteKey));
  expectFalse(historyModel.isDeletingVisits());

  expectEquals(visit.titleLink, document.activeElement);
  testDone();
});

/**
 * Fixture for History WebUI testing IDN.
 * @extends {BaseHistoryWebUITest}
 * @constructor
 */
function HistoryWebUIIDNTest() {}

HistoryWebUIIDNTest.prototype = {
  __proto__: BaseHistoryWebUITest.prototype,

  /** @override */
  testGenPreamble: function() {
    // Add some visits to the history database.
    GEN('  AddPageToHistory(0, "http://xn--d1abbgf6aiiy.xn--p1ai/",' +
        ' "Some");');

    // Clear AcceptLanguages to get domain in unicode.
    GEN('  ClearAcceptLanguages();');
  },
};

/**
 * Simple test that verifies that the correct entries are retrieved from the
 * history database and displayed in the UI.
 */
// Times out on Mac: http://crbug.com/336845
TEST_F('HistoryWebUIIDNTest', 'DISABLED_basic', function() {
  // Check that there is only one entry and domain is in unicode.
  assertEquals(1, document.querySelectorAll('.domain').length);
  assertEquals("\u043f\u0440\u0435\u0437\u0438\u0434\u0435\u043d\u0442." +
               "\u0440\u0444", document.querySelector('.domain').textContent);

  testDone();
});

/**
 * Fixture for a test that uses the real backend and tests how the history
 * page deals with odd schemes in URLs.
 * @extends {HistoryWebUIRealBackendTest}
 */
function HistoryWebUIWithSchemesTest() {}

HistoryWebUIWithSchemesTest.prototype = {
  __proto__: HistoryWebUIRealBackendTest.prototype,

  /** @override */
  testGenPreamble: function() {
    // Add a bunch of entries on the same day, including some weird schemes.
    GEN('  AddPageToHistory(12, "http://google.com", "Google");');
    GEN('  AddPageToHistory(13, "file:///tmp/foo", "");');
    GEN('  AddPageToHistory(14, "mailto:chromium@chromium.org", "");');
    GEN('  AddPageToHistory(15, "tel:555123456", "");');
  },

  setUp: function() {
    // Show the filter controls as if the command line switch was active.
    $('top-container').hidden = true;
    $('history-page').classList.add('big-topbar-page');
    $('filter-controls').hidden = false;
    expectFalse($('filter-controls').hidden);
  },
};

TEST_F('HistoryWebUIWithSchemesTest', 'groupingWithSchemes', function() {
  // Switch to the week view.
  $('timeframe-controls').querySelectorAll('input')[1].click();
  waitForCallback('historyResult', function() {
    // Each URL should be organized under a different "domain".
    expectEquals(document.querySelectorAll('.entry').length, 4);
    expectEquals(document.querySelectorAll('.site-domain-wrapper').length, 4);
    testDone();
  });
});
