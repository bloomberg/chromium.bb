// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

metrics.decorate = function(name) {
  this[name] = function() {
    var args = Array.apply(null, arguments);
    args[0] = metrics.convertName_(args[0]);
    chrome.metricsPrivate[name].apply(chrome.metricsPrivate, args);
    if (localStorage.logMetrics) {
      console.log('chrome.metricsPrivate.' + name, args);
    }
  }
};

metrics.decorate('recordMediumCount');
metrics.decorate('recordSmallCount');
metrics.decorate('recordTime');
metrics.decorate('recordUserAction');

metrics.recordInterval = function(name) {
  if (name in metrics.intervals) {
    metrics.recordTime(name, Date.now() - metrics.intervals[name]);
  } else {
    console.error('Unknown interval: ' + name);
  }
};

metrics.recordEnum = function(name, value, validValues) {
  var boundaryValue;
  var index;
  if (validValues.constructor.name == 'Array') {
    index = validValues.indexOf(value);
    boundaryValue = validValues.length;
  } else {
    index = value;
    boundaryValue = validValues;
  }
  // Collect invalid values in the overflow bucket at the end.
  if (index < 0 || index > boundaryValue)
    index = boundaryValue;

  // Setting min to 1 looks strange but this is exactly the recommended way
  // of using histograms for enum-like types. Bucket #0 works as a regular
  // bucket AND the underflow bucket.
  // (Source: UMA_HISTOGRAM_ENUMERATION definition in base/metrics/histogram.h)
  var metricDescr = {
    'metricName': metrics.convertName_(name),
    'type': 'histogram-linear',
    'min': 1,
    'max': boundaryValue,
    'buckets': boundaryValue + 1
  };
  chrome.metricsPrivate.recordValue(metricDescr, index);
  if (localStorage.logMetrics) {
    console.log('chrome.metricsPrivate.recordValue',
        [metricDescr.metricName, index, value]);
  }
};
