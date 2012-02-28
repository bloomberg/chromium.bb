// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var timeutil = (function() {
  'use strict';

  /**
   * Offset needed to convert event times to Date objects.
   * Updated whenever constants are loaded.
   */
  var timeTickOffset = 0;

  /**
   * Sets the offset used to convert tick counts to dates.
   */
  function setTimeTickOffset(offset) {
    // Note that the subtraction by 0 is to cast to a number (probably a float
    // since the numbers are big).
    timeTickOffset = offset - 0;
  }

  /**
   * The browser gives us times in terms of "time ticks" in milliseconds.
   * This function converts the tick count to a Date() object.
   *
   * @param {String} timeTicks.
   * @returns {Date} The time that |timeTicks| represents.
   */
  function convertTimeTicksToDate(timeTicks) {
    var timeStampMs = timeTickOffset + (timeTicks - 0);
    return new Date(timeStampMs);
  }

  /**
   * Returns the current time.
   *
   * @returns {number} Milliseconds since the Unix epoch.
   */
  function getCurrentTime() {
    return (new Date()).getTime();
  }

  /**
   * Adds an HTML representation of |date| to |parentNode|.
   *
   * @param {DomNode} parentNode
   * @param {Date} date
   * @returns {DomNode} The node containing the date/time.
   */
  function addNodeWithDate(parentNode, date) {
    var span = addNodeWithText(parentNode, 'span', dateToString(date));
    span.title = 't=' + date.getTime();
    return span;
  }

  /**
   * Returns a string representation of |date|.
   *
   * @param {Date} date
   * @returns {String}
   */
  function dateToString(date) {
    var dateStr =
        date.getFullYear() + '/' + (date.getMonth() + 1) + '/' + date.getDate();

    // Prefix the milliseconds with enough zeros to make it three characters
    // long.
    var paddedMilliseconds = '' + date.getMilliseconds();
    while (paddedMilliseconds.length < 3)
      paddedMilliseconds = '0' + paddedMilliseconds;

    var timeStr =
        date.getHours() + ':' + date.getMinutes() + ':' + date.getSeconds() +
        '.' + paddedMilliseconds;

    return '[' + dateStr + '] ' + timeStr;
  }

  return {
    setTimeTickOffset: setTimeTickOffset,
    convertTimeTicksToDate: convertTimeTicksToDate,
    getCurrentTime: getCurrentTime,
    addNodeWithDate: addNodeWithDate,
    dateToString: dateToString
  };
})();
