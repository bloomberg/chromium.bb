// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EventsTracker = (function() {
  'use strict';

  /**
   * This class keeps track of all NetLog events.
   * It receives events from the browser and when loading a log file, and passes
   * them on to all its observers.
   *
   * @constructor
   */
  function EventsTracker() {
    assertFirstConstructorCall(EventsTracker);

    this.capturedEvents_ = [];
    this.observers_ = [];
  }

  cr.addSingletonGetter(EventsTracker);

  EventsTracker.prototype = {
    /**
     * Returns a list of all captured events.
     */
    getAllCapturedEvents: function() {
      return this.capturedEvents_;
    },

    /**
     * Returns the number of events that were captured.
     */
    getNumCapturedEvents: function() {
      return this.capturedEvents_.length;
    },

    /**
     * Deletes all the tracked events, and notifies any observers.
     */
    deleteAllLogEntries: function() {
      this.capturedEvents_ = [];
      for (var i = 0; i < this.observers_.length; ++i)
        this.observers_[i].onAllLogEntriesDeleted();
    },

    /**
     * Adds captured events, and broadcasts them to any observers.
     */
    addLogEntries: function(logEntries) {
      this.capturedEvents_ = this.capturedEvents_.concat(logEntries);
      for (var i = 0; i < this.observers_.length; ++i) {
        this.observers_[i].onReceivedLogEntries(logEntries);
      }
    },

    /**
     * Adds a listener of log entries. |observer| will be called back when new
     * log data arrives or all entries are deleted:
     *
     *   observer.onReceivedLogEntries(entries)
     *   observer.onAllLogEntriesDeleted()
     */
    addLogEntryObserver: function(observer) {
      this.observers_.push(observer);
    }
  };

  return EventsTracker;
})();
