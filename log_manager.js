// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Log manager which enables and collects both fine and info logs,
 *    and provides method to get logs with parameter to include or exclude
 *    fine logs.

 */

goog.provide('mr.LogManager');

goog.require('mr.Config');
goog.require('mr.FixedSizeQueue');
goog.require('mr.Logger');
goog.require('mr.PersistentData');
goog.require('mr.PersistentDataManager');


/**
 * @implements {mr.PersistentData}
 */
mr.LogManager = class {
  constructor() {
    /** @private @const */
    this.buffer_ = new mr.FixedSizeQueue(mr.LogManager.BUFFER_SIZE);

    /** @private @const */
    this.startTime_ = Date.now();
  }

  /**
   * @return {!mr.LogManager}
   */
  static getInstance() {
    if (mr.LogManager.instance_ == null) {
      mr.LogManager.instance_ = new mr.LogManager();
    }
    return mr.LogManager.instance_;
  }

  /**
   * Init
   */
  init() {
    mr.Logger.level = this.getDefaultLogLevel_();
    const browserLogger = mr.Logger.getInstance('browser');
    const oldErrorHandler = window.onerror;
    /**
     * @param {string} message
     * @param {string} url
     * @param {number} line
     * @param {number=} col
     * @param {*=} error
     */
    window.onerror = (message, url, line, col, error) => {
      if (oldErrorHandler) {
        oldErrorHandler(message, url, line, col, error);
      }
      browserLogger.error(`Error: ${message} (${url} @ Line: ${line})`, error);
    };
    mr.Logger.addHandler(this.onNewLog_.bind(this));

    // Override log level via localStorage setting
    const debugKey = 'debug.logs';
    const debugLevel = window.localStorage[debugKey];
    if (debugLevel) {
      mr.Logger.level = mr.Logger.stringToLevel(
          debugLevel.toUpperCase(), mr.Logger.Level.FINE);
    } else if (!mr.Config.isPublicChannel) {
      // Record the default local level in local settings so developers can
      // easily change it without having to look up the name of the setting.
      window.localStorage[debugKey] =
          mr.Logger.levelToString(mr.Logger.DEFAULT_LEVEL);
    }

    const consoleKey = 'debug.console';
    if (!mr.Config.isPublicChannel && window.localStorage[consoleKey] == null) {
      // Enable console logging by default in internal builds.  Any value other
      // than 'false' or '' is treated as true.
      window.localStorage[consoleKey] = 'true';
    }
    const consoleValue = window.localStorage[consoleKey];
    if (consoleValue && consoleValue.toLowerCase() != 'false') {
      mr.Logger.addHandler(this.logToConsole_.bind(this));
    }
  }

  /**
   * Saves logs in the internal buffer.
   *
   * @param {mr.Logger.Record} logRecord The log entry.
   * @private
   */
  onNewLog_(logRecord) {
    this.buffer_.enqueue(this.formatRecord_(logRecord, false));
    const exception = logRecord.exception;
    if (exception instanceof Error && exception.stack) {
      this.buffer_.enqueue(exception.stack);
    }
  }

  /**
   * @param {mr.Logger.Record} logRecord The log entry.
   * @private
   */
  logToConsole_(logRecord) {
    const args = [this.formatRecord_(logRecord, true)];
    if (logRecord.exception) {
      args.push(logRecord.exception);
    }
    switch (logRecord.level) {
      case mr.Logger.Level.SEVERE:
        console.error(...args);
        break;
      case mr.Logger.Level.WARNING:
        console.warn(...args);
        break;
      case mr.Logger.Level.INFO:
        console.log(...args);
        break;
      default:
        console.debug(...args);
    }
  }

  /**
   * @param {!mr.Logger.Record} record
   * @param {boolean} forConsole
   * @return {string}
   * @private
   */
  formatRecord_(record, forConsole) {
    const sb = ['['];
    if (forConsole) {
      // Format relative timestamp.
      const seconds = (Date.now() - this.startTime_) / 1000;
      sb.push(('       ' + seconds.toFixed(3)).slice(-7));
    } else {
      // Format absolute timestamp.
      const date = new Date(record.time);
      const twoDigitStr = num => num < 10 ? '0' + num : num;
      sb.push(
          date.getFullYear().toString(), '-', twoDigitStr(date.getMonth() + 1),
          '-', twoDigitStr(date.getDate()), ' ', twoDigitStr(date.getHours()),
          ':', twoDigitStr(date.getMinutes()), ':',
          twoDigitStr(date.getSeconds()), '.',
          twoDigitStr(Math.floor(date.getMilliseconds() / 10)));
    }
    sb.push(
        '][', mr.Logger.levelToString(record.level), '][', record.logger, '] ',
        record.message);
    // Don't append the exception when logging to the console, because it will
    // be handled specially later.
    if (!forConsole && record.exception != null) {
      sb.push('\n');
      if (record.exception instanceof Error) {
        sb.push(record.exception.message);
      } else {
        try {
          sb.push(JSON.stringify(record.exception));
        } catch (e) {
          sb.push(record.exception.toString());
        }
      }
    }
    sb.push('\n');
    return sb.join('');
  }

  /**
   * Get the logs in log buffer.
   * @return {string}
   */
  getLogs() {
    if (this.buffer_.getCount() == 0) {
      return 'NA';
    }

    return this.buffer_.getValues().join('');
  }

  /**
   * @return {!mr.Logger.Level} The default log level.
   * @private
   */
  getDefaultLogLevel_() {
    return mr.Config.isPublicChannel ? mr.Logger.Level.INFO :
                                       mr.Logger.Level.FINE;
  }

  /**
   * Registers with the data manager and loads any previous logs.
   */
  registerDataManager() {
    mr.PersistentDataManager.register(this);
  }

  /**
   * @override
   */
  getStorageKey() {
    return 'LogManager';
  }

  /**
   * @override
   */
  getData() {
    return [this.buffer_.getValues()];
  }

  /**
   * @override
   */
  loadSavedData() {
    const currentLogs = this.buffer_.getValues();
    this.buffer_.clear();
    for (let log of mr.PersistentDataManager.getTemporaryData(this) || []) {
      this.buffer_.enqueue(log);
    }
    for (let log of currentLogs) {
      this.buffer_.enqueue(log);
    }
  }
};


/**
 * @private {mr.LogManager}
 */
mr.LogManager.instance_ = null;


/**
 * The max number of logs in buffer. The old logs get pushed out when the buffer
 * is full.
 * @const
 */
mr.LogManager.BUFFER_SIZE = 1000;
