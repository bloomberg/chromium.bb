// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Store ChromeVox log.
 */

goog.provide('LogStore');
goog.provide('Log');

/**
 * @param {!string} logStr
 * @param {!string} logType
 * @constructor
 */
Log = function(logStr, logType) {
  /**
   * @type {!string}
   */
  this.logStr = logStr;

  /**
   * @type {!string}
   */
  this.logType = logType;

  /**
   * @type {!Date}
   */
  this.date = new Date();
};

/**
 * @constructor
 */
LogStore = function() {
  /**
   * Ring buffer of size this.LOG_LIMIT
   * @type {!Array<Log>}
   * @private
   */
  this.logs_ = Array(LogStore.LOG_LIMIT);

  /*
   * this.logs_ is implemented as a ring buffer which starts
   * from this.startIndex_ and ends at this.startIndex_-1
   * In the initial state, this array is filled by undefined.
   * @type {number}
   * @private
   */
  this.startIndex_ = 0;
};

/** @enum {string} */
LogStore.LogType = {
  SPEECH: 'speech',
  EARCON: 'earcon',
  BRAILLE: 'braille',
  EVENT: 'event',
};

/**
 * @const
 * @private
 */
LogStore.LOG_LIMIT = 3000;

/**
 * Create logs in order.
 * This is not the best way to create logs fast but
 * getLogs() is not called often.
 * @return {!Array<Log>}
 */
LogStore.prototype.getLogs = function() {
  var returnLogs = [];
  for (var i = 0; i < LogStore.LOG_LIMIT; i++) {
    var index = (this.startIndex_ + i) % LogStore.LOG_LIMIT;
    if (!this.logs_[index])
      continue;
    returnLogs.push(this.logs_[index]);
  }
  return returnLogs;
};

/**
 * Write a log to this.logs_.
 * To add a message to logs, this function shuold be called.
 * @param {!string} logStr
 * @param {!LogStore.LogType} logType
 */
LogStore.prototype.writeLog = function(logStr, logType) {
  var log = new Log(logStr, logType);
  this.logs_[this.startIndex_] = log;
  this.startIndex_ += 1;
  if (this.startIndex_ == LogStore.LOG_LIMIT)
    this.startIndex_ = 0;
};

/**
 * Clear this.logs_.
 * Set to initial states.
 */
LogStore.prototype.clearLog = function() {
  this.logs_ = Array(LogStore.LOG_LIMIT);
  this.startIndex_ = 0;
};

/**
 * Global instance.
 * @type {LogStore}
 */
LogStore.instance;

/**
 * @return {LogStore}
 */
LogStore.getInstance = function() {
  if (!LogStore.instance)
    LogStore.instance = new LogStore();
  return LogStore.instance;
};
