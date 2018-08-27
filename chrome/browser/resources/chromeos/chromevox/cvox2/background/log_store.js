// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Store ChromeVox log.
 *
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
};

/**
 * @constructor
 */
LogStore = function() {
  /**
   * @type {!Array<Log>}
   */
  this.logs_ = [];

  /**
   * @const {number}
   */
  this.limitLog_ = 3000;
};


/**
 * @enum {string}
 */
LogStore.LogType = {
  SPEECH: 'speech',
  EARCON: 'earcon',
  BRAILLE: 'braille',
  EVENT: 'event',
};

/**
 * @return {!Array<Log>}
 */
LogStore.prototype.getLogs = function() {
  return this.logs_;
};

/**
 * @param {!string} logStr
 * @param {!LogStore.LogType} logType
 */
LogStore.prototype.writeLog = function(logStr, logType) {
  var log = new Log(logStr, logType);
  this.logs_.push(log);
  // TODO(elkurin): shift() takes O(n).
  // To improve performance, implement QUEUE.
  if (this.logs_.length > this.limitLog_)
    this.logs_.shift();
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
  if (!LogStore.instance) {
    LogStore.instance = new LogStore();
  }
  return LogStore.instance;
};
