// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// metrics api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Metrics

// Any changes to the logging done in these functions should be matched
// with the checks done in IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Metrics).
// See extension_metrics_apitest.cc.
chrome.test.runTests([
  function recordUserAction() {
    // Log a metric once.
    chrome.metrics.recordUserAction('test.ua.1');

    // Log a metric more than once.
    chrome.metrics.recordUserAction('test.ua.2');
    chrome.metrics.recordUserAction('test.ua.2');

    chrome.test.succeed();
  },

  function recordValue() {
    chrome.metrics.recordValue({
      'metricName': 'test.h.1',
      'type': 'histogram-log',
      'min': 1,
      'max': 100,
      'buckets': 50
    }, 42);

    chrome.metrics.recordValue({
      'metricName': 'test.h.2',
      'type': 'histogram-linear',
      'min': 1,
      'max': 200,
      'buckets': 50
    }, 42);

    chrome.metrics.recordPercentage('test.h.3', 42);
    chrome.metrics.recordPercentage('test.h.3', 42);

    chrome.test.succeed();
  },

  function recordTimes() {
    chrome.metrics.recordTime('test.time', 42);
    chrome.metrics.recordMediumTime('test.medium.time', 42 * 1000);
    chrome.metrics.recordLongTime('test.long.time', 42 * 1000 * 60);

    chrome.test.succeed();
  },

  function recordCounts() {
    chrome.metrics.recordCount('test.count', 420000);
    chrome.metrics.recordMediumCount('test.medium.count', 4200);
    chrome.metrics.recordSmallCount('test.small.count', 42);

    chrome.test.succeed();
  }
]);

