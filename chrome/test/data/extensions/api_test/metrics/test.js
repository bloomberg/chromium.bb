// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
    chrome.metricsPrivate.recordUserAction('test.ua.1');

    // Log a metric more than once.
    chrome.metricsPrivate.recordUserAction('test.ua.2');
    chrome.metricsPrivate.recordUserAction('test.ua.2');

    chrome.test.succeed();
  },

  function recordValue() {
    chrome.metricsPrivate.recordValue({
      'metricName': 'test.h.1',
      'type': 'histogram-log',
      'min': 1,
      'max': 100,
      'buckets': 50
    }, 42);

    chrome.metricsPrivate.recordValue({
      'metricName': 'test.h.2',
      'type': 'histogram-linear',
      'min': 1,
      'max': 200,
      'buckets': 50
    }, 42);

    chrome.metricsPrivate.recordPercentage('test.h.3', 42);
    chrome.metricsPrivate.recordPercentage('test.h.3', 42);

    chrome.test.succeed();
  },

  function recordTimes() {
    chrome.metricsPrivate.recordTime('test.time', 42);
    chrome.metricsPrivate.recordMediumTime('test.medium.time', 42 * 1000);
    chrome.metricsPrivate.recordLongTime('test.long.time', 42 * 1000 * 60);

    chrome.test.succeed();
  },

  function recordCounts() {
    chrome.metricsPrivate.recordCount('test.count', 420000);
    chrome.metricsPrivate.recordMediumCount('test.medium.count', 4200);
    chrome.metricsPrivate.recordSmallCount('test.small.count', 42);

    chrome.test.succeed();
  }
]);

