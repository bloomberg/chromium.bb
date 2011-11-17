// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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

  return {
    setTimeTickOffset: setTimeTickOffset,
    convertTimeTicksToDate: convertTimeTicksToDate,
    getCurrentTime: getCurrentTime
  };
})();
