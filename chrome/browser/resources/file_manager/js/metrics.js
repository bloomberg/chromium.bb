// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

var metrics = {};

metrics.intervals = {};

metrics.startInterval = function(name) {
  metrics.intervals[name] = Date.now();
};

metrics.startInterval('Load.Total');
metrics.startInterval('Load.Script');

metrics.convertName_ = function(name) {
  return 'FileBrowser.' + name;
};

metrics.recordTime = function(name) {
  if (name in metrics.intervals) {
    var elapsed = Date.now() - metrics.intervals[name];
    console.log(name + ': ' + elapsed + 'ms');
    chrome.metricsPrivate.recordTime(metrics.convertName_(name), elapsed);
  } else {
    console.error('Unknown interval: ' + name);
  }
};

metrics.recordAction = function(name) {
  chrome.metricsPrivate.recordUserAction(metrics.convertName_(name));
};

metrics.reportCount = function(name, value) {
  chrome.metricsPrivate.recordMediumCount(metrics.convertName_(name), value);
};

metrics.recordEnum = function(name, value, validValues) {
  var index = validValues.indexOf(value);

  // Collect invalid values in the extra bucket at the end.
  if (index < 0) index = validValues.length;

  chrome.metricsPrivate.recordValue({
      'metricName': metrics.convertName_(name),
      'type': 'histogram-linear',
      'min': 0,
      'max': validValues.length,
      'buckets': validValues.length + 1
    },
    index);
};
