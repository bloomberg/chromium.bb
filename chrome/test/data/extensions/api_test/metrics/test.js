// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// metrics api test
// browser_tests.exe --gtest_filter=ExtensionApiTest.Metrics

// Any changes to the logging done in these functions should be matched
// with the checks done in IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Metrics).
// See extension_metrics_apitest.cc.
chrome.test.runTests([
  function getSetEnabled() {
    var pass = chrome.test.callbackPass;
    var metrics = chrome.experimental.metrics;
    // Try to change the setting and put it back. We use callbacks to ensure
    // all functions are called in order.
    metrics.getEnabled(pass(function(old_enabled) {
      // Verify that it is, indeed, a boolean.
      chrome.test.assertEq(old_enabled, !!old_enabled);
      metrics.setEnabled(!old_enabled, pass(function(new_enabled) {
        chrome.test.assertEq(old_enabled, !new_enabled);
        metrics.getEnabled(pass(function(n) {
          chrome.test.assertEq(n, new_enabled);

          metrics.setEnabled(old_enabled, pass(function(o) {
            chrome.test.assertEq(o, old_enabled);
          }));
        }));
      }));
    }));
  },

  function recordUserAction() {
    // Log a metric once.
    chrome.experimental.metrics.recordUserAction('test.ua.1');

    // Log a metric more than once.
    chrome.experimental.metrics.recordUserAction('test.ua.2');
    chrome.experimental.metrics.recordUserAction('test.ua.2');

    chrome.test.succeed();
  },

  function recordValue() {
    chrome.experimental.metrics.recordValue({
      'metricName': 'test.h.1',
      'type': 'histogram-log',
      'min': 1,
      'max': 100,
      'buckets': 50
    }, 42);

    chrome.experimental.metrics.recordValue({
      'metricName': 'test.h.2',
      'type': 'histogram-linear',
      'min': 1,
      'max': 200,
      'buckets': 50
    }, 42);

    chrome.experimental.metrics.recordPercentage('test.h.3', 42);
    chrome.experimental.metrics.recordPercentage('test.h.3', 42);

    chrome.test.succeed();
  },

  function recordTimes() {
    chrome.experimental.metrics.recordTime('test.time', 42);
    chrome.experimental.metrics.recordMediumTime('test.medium.time', 42 * 1000);
    chrome.experimental.metrics.recordLongTime('test.long.time',
                                               42 * 1000 * 60);

    chrome.test.succeed();
  },

  function recordCounts() {
    chrome.experimental.metrics.recordCount('test.count', 420000);
    chrome.experimental.metrics.recordMediumCount('test.medium.count', 4200);
    chrome.experimental.metrics.recordSmallCount('test.small.count', 42);

    chrome.test.succeed();
  }
]);

