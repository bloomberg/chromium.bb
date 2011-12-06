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
  var maxValue;
  var index;
  if (validValues.constructor.name == 'Array') {
    index = validValues.indexOf(value);
    maxValue = validValues.length - 1;
  } else {
    index = value;
    maxValue = validValues - 1;
  }
  // Collect invalid values in the extra bucket at the end.
  if (index < 0 || index > maxValue)
    index = maxValue;

  var metricDescr = {
    'metricName': metrics.convertName_(name),
    'type': 'histogram-linear',
    'min': 0,
    'max': maxValue,
    'buckets': maxValue + 1
  };
  chrome.metricsPrivate.recordValue(metricDescr, index);
  if (localStorage.logMetrics) {
    console.log('chrome.metricsPrivate.recordValue',
        [metricDescr.metricName, index, value]);
  }
};
