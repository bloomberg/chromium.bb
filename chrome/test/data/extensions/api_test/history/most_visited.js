// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryExtensionApiTest.MostVisited

var GOOGLE_ENTRY = {'url' : GOOGLE_URL, 'title' : 'Google'};
var PICASA_ENTRY = {'url' : PICASA_URL, 'title' : 'Picasa'};
var HALF_HOUR_MILLISECONDS = 30 * 60 * 1000;
var ONE_HOUR_MILLISECONDS = 60 * 60 * 1000;

function getVerificationFunction(expected) {
  return function(results) {
    chrome.test.assertEq(expected, results);
    chrome.test.succeed();
  }
}

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  function mostVisitedTimeRange1() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('24 Apr 2012, 13:00:00'),
      'filterWidth': HALF_HOUR_MILLISECONDS,
    }, getVerificationFunction([GOOGLE_ENTRY]));
  },
  function mostVisitedTimeRange2() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('24 Apr 2012, 14:00:00'),
      'filterWidth': HALF_HOUR_MILLISECONDS,
    }, getVerificationFunction([PICASA_ENTRY]));
  },
  function mostVisitedTimeRange3() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('24 Apr 2012, 12:00:00'),
      'filterWidth': HALF_HOUR_MILLISECONDS,
    }, getVerificationFunction([]));
  },
  function mostVisitedTimeRange4() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('24 Apr 2012, 15:00:00'),
      'filterWidth': HALF_HOUR_MILLISECONDS,
    }, getVerificationFunction([]));
  },
  function mostVisitedTimeRange5() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('24 Apr 2012, 13:30:00'),
      'filterWidth': ONE_HOUR_MILLISECONDS,
    }, getVerificationFunction([PICASA_ENTRY, GOOGLE_ENTRY]));
  },
  function mostVisitedMaxResults() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('24 Apr 2012, 13:30:00'),
      'filterWidth': ONE_HOUR_MILLISECONDS,
      maxResults: 1,
    }, getVerificationFunction([PICASA_ENTRY]));
  },
  function mostVisitedDayOfTheWeekRightDay() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('25 Apr 2012, 13:30:00'),
      'dayOfTheWeek': 2,
    }, getVerificationFunction([PICASA_ENTRY, GOOGLE_ENTRY]));
  },
  function mostVisitedDayOfTheWeekWrongDay() {
    chrome.experimental.history.getMostVisited({
      'filterTime': Date.parse('25 Apr 2012, 13:30:00'),
      'dayOfTheWeek': 1,
    }, getVerificationFunction([]));
  }
]);
