// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

var metrics = {};

/**
 * A map from interval name to interval start timestamp.
 */
metrics.intervals = {};

/**
 * Start the named time interval.
 * Should be followed by a call to recordInterval with the same name.
 *
 * @param {string} name Unique interval name.
 */
metrics.startInterval = function(name) {
  metrics.intervals[name] = Date.now();
};

metrics.startInterval('Load.Total');
metrics.startInterval('Load.Script');

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @private
 */
metrics.convertName_ = function(name) {
  return 'FileBrowser.' + name;
};

/**
 * Wrapper method for calling chrome.fileBrowserPrivate safely.
 * @param {string} name Method name.
 * @param {Array.<Object>} args Arguments.
 * @private
 */
metrics.call_ = function(name, args) {
  try {
    chrome.metricsPrivate[name].apply(chrome.metricsPrivate, args);
  } catch (e) {
    console.error(e.stack);
  }
};

/**
 * Create a decorator function that calls a chrome.metricsPrivate function
 * with the same name and correct parameters.
 *
 * @param {string} name Method name.
 */
metrics.decorate = function(name) {
  metrics[name] = function() {
    var args = Array.apply(null, arguments);
    args[0] = metrics.convertName_(args[0]);
    metrics.call_(name, args);
    if (metrics.log) {
      console.log('chrome.metricsPrivate.' + name, args);
    }
  };
};

metrics.decorate('recordMediumCount');
metrics.decorate('recordSmallCount');
metrics.decorate('recordTime');
metrics.decorate('recordUserAction');

/**
 * Complete the time interval recording.
 *
 * Should be preceded by a call to startInterval with the same name. *
 *
 * @param {string} name Unique interval name.
 */
metrics.recordInterval = function(name) {
  if (name in metrics.intervals) {
    metrics.recordTime(name, Date.now() - metrics.intervals[name]);
  } else {
    console.error('Unknown interval: ' + name);
  }
};

/**
 * Record an enum value.
 *
 * @param {string} name Metric name.
 * @param {Object} value Enum value.
 * @param {Array.<Object>|number} validValues Array of valid values
 *                                            or a boundary number value.
 */
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
  metrics.call_('recordValue', [metricDescr, index]);
  if (metrics.log) {
    console.log('chrome.metricsPrivate.recordValue',
        [metricDescr.metricName, index, value]);
  }
};
