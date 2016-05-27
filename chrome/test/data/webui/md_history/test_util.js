// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Waits for queued up tasks to finish before proceeding. Inspired by:
 * https://github.com/Polymer/web-component-tester/blob/master/browser/environment/helpers.js#L97
 */
function flush() {
  Polymer.dom.flush();
  // Promises have microtask timing, so we use setTimeout to explicity force a
  // new task.
  return new Promise(function(resolve, reject) {
    window.setTimeout(resolve, 0);
  });
}

/**
 * Create a fake history result with the given timestamp.
 * @param {number|string} timestamp Timestamp of the entry, as a number in ms or
 * a string which can be parsed by Date.parse().
 * @param {string} urlStr The URL to set on this entry.
 * @return {!HistoryEntry} An object representing a history entry.
 */
function createHistoryEntry(timestamp, urlStr) {
  if (typeof timestamp === 'string')
    timestamp += ' UTC';

  var d = new Date(timestamp);
  var url = new URL(urlStr);
  var domain = url.host;
  return {
    allTimestamps: [timestamp],
    // Formatting the relative day is too hard, will instead display
    // YYYY-MM-DD.
    dateRelativeDay: d.toISOString().split('T')[0],
    dateTimeOfDay: d.getUTCHours() + ':' + d.getUTCMinutes(),
    domain: domain,
    starred: false,
    time: d.getTime(),
    title: urlStr,
    url: urlStr
  };
}

/**
 * Create a fake history search result with the given timestamp. Replaces fields
 * from createHistoryEntry to look like a search result.
 * @param {number|string} timestamp Timestamp of the entry, as a number in ms or
 * a string which can be parsed by Date.parse().
 * @param {string} urlStr The URL to set on this entry.
 * @return {!HistoryEntry} An object representing a history entry.
 */
function createSearchEntry(timestamp, urlStr) {
  var entry = createHistoryEntry(timestamp, urlStr);
  entry.dateShort = entry.dateRelativeDay;
  entry.dateTimeOfDay = '';
  entry.dateRelativeDay = '';

  return entry;
}

/**
 * Create a simple HistoryInfo.
 * @param {?string} searchTerm The search term that the info has. Will be empty
 *     string if not specified.
 * @return {!HistoryInfo}
 */
function createHistoryInfo(searchTerm) {
  return {
    finished: true,
    hasSyncedResults: false,
    queryEndTime: 'Monday',
    queryStartTime: 'Tuesday',
    term: searchTerm || ''
  };
}
