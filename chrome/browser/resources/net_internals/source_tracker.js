// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var SourceTracker = (function() {
  'use strict';

  /**
   * This class keeps track of all NetLog events.
   * It receives events from the browser and when loading a log file, and passes
   * them on to all its observers.
   *
   * @constructor
   */
  function SourceTracker() {
    // Observers that are sent all events as they happen.  This allows for easy
    // watching for particular events.
    this.logEntryObservers_ = [];

    // Observers that only want to receive lists of updated SourceEntries.
    this.sourceEntryObservers_ = [];

    // True when cookies and authentication information should be removed from
    // displayed events.  When true, such information should be hidden from
    // all pages.
    this.enableSecurityStripping_ = true;

    this.clearEntries_();
  }

  SourceTracker.prototype = {
    /**
     * Clears all log entries and SourceEntries and related state.
     */
    clearEntries_: function() {
      // Used for sorting entries with automatically assigned IDs.
      this.maxReceivedSourceId_ = 0;

      // Next unique id to be assigned to a log entry without a source.
      // Needed to simplify deletion, identify associated GUI elements, etc.
      this.nextSourcelessEventId_ = -1;

      this.numPassivelyCapturedEvents_ = 0;

      // Ordered list of log entries.  Needed to maintain original order when
      // generating log dumps
      this.capturedEvents_ = [];

      this.sourceEntries_ = {};
    },

    /**
     * Returns a list of all captured events.
     */
    getAllCapturedEvents: function() {
      return this.capturedEvents_;
    },

    /**
     * Returns a list of all SourceEntries.
     */
    getAllSourceEntries: function() {
      return this.sourceEntries_;
    },

    /**
     * Returns the number of events that were captured while we were
     * listening for events.
     */
    getNumActivelyCapturedEvents: function() {
      return this.capturedEvents_.length - this.numPassivelyCapturedEvents_;
    },

    /**
     * Returns the number of events that were captured passively by the
     * browser prior to when the net-internals page was started.
     */
    getNumPassivelyCapturedEvents: function() {
      return this.numPassivelyCapturedEvents_;
    },

    /**
     * Returns the description of the specified SourceEntry, or an empty string
     * if it doesn't exist.
     */
    getDescription: function(id) {
      var entry = this.getSourceEntry(id);
      if (entry)
        return entry.getDescription();
      return '';
    },

    /**
     * Returns the specified SourceEntry.
     */
    getSourceEntry: function(id) {
      return this.sourceEntries_[id];
    },

    onReceivedPassiveLogEntries: function(logEntries) {
      // Due to an expected race condition, it is possible to receive actively
      // captured log entries before the passively logged entries are received.
      //
      // When that happens, we create a copy of the actively logged entries,
      // delete all entries, and, after handling all the passively logged
      // entries, add back the deleted actively logged entries.
      var earlyActivelyCapturedEvents = this.capturedEvents_.slice(0);
      if (earlyActivelyCapturedEvents.length > 0)
        this.deleteAllSourceEntries();

      this.numPassivelyCapturedEvents_ = logEntries.length;
      for (var i = 0; i < logEntries.length; ++i)
        logEntries[i].wasPassivelyCaptured = true;

      this.onReceivedLogEntries(logEntries);

      // Add back early actively captured events, if any.
      if (earlyActivelyCapturedEvents.length)
        this.onReceivedLogEntries(earlyActivelyCapturedEvents);
    },

    /**
     * Sends each entry to all observers and updates |capturedEvents_|.
     * Also assigns unique ids to log entries without a source.
     */
    onReceivedLogEntries: function(logEntries) {
      // List source entries with new log entries.  Sorted chronologically, by
      // first new log entry.
      var updatedSourceEntries = [];

      var updatedSourceEntryIdMap = {};

      for (var e = 0; e < logEntries.length; ++e) {
        var logEntry = logEntries[e];

        // Assign unique ID, if needed.
        if (logEntry.source.id == 0) {
          logEntry.source.id = this.nextSourcelessEventId_;
          --this.nextSourcelessEventId_;
        } else if (this.maxReceivedSourceId_ < logEntry.source.id) {
          this.maxReceivedSourceId_ = logEntry.source.id;
        }

        // Create/update SourceEntry object.
        var sourceEntry = this.sourceEntries_[logEntry.source.id];
        if (!sourceEntry) {
          sourceEntry = new SourceEntry(logEntry, this.maxReceivedSourceId_);
          this.sourceEntries_[logEntry.source.id] = sourceEntry;
        } else {
          sourceEntry.update(logEntry);
        }

        // Add to updated SourceEntry list, if not already in it.
        if (!updatedSourceEntryIdMap[logEntry.source.id]) {
          updatedSourceEntryIdMap[logEntry.source.id] = sourceEntry;
          updatedSourceEntries.push(sourceEntry);
        }
      }

      this.capturedEvents_ = this.capturedEvents_.concat(logEntries);
      for (var i = 0; i < this.sourceEntryObservers_.length; ++i) {
        this.sourceEntryObservers_[i].onSourceEntriesUpdated(
            updatedSourceEntries);
      }
      for (var i = 0; i < this.logEntryObservers_.length; ++i)
        this.logEntryObservers_[i].onReceivedLogEntries(logEntries);
    },

    /**
     * Deletes captured events with source IDs in |sourceEntryIds|.
     */
    deleteSourceEntries: function(sourceEntryIds) {
      var sourceIdDict = {};
      for (var i = 0; i < sourceEntryIds.length; i++) {
        sourceIdDict[sourceEntryIds[i]] = true;
        delete this.sourceEntries_[sourceEntryIds[i]];
      }

      var newEventList = [];
      for (var i = 0; i < this.capturedEvents_.length; ++i) {
        var id = this.capturedEvents_[i].source.id;
        if (id in sourceIdDict) {
          if (this.capturedEvents_[i].wasPassivelyCaptured)
            --this.numPassivelyCapturedEvents_;
          continue;
        }
        newEventList.push(this.capturedEvents_[i]);
      }
      this.capturedEvents_ = newEventList;

      for (var i = 0; i < this.sourceEntryObservers_.length; ++i)
        this.sourceEntryObservers_[i].onSourceEntriesDeleted(sourceEntryIds);
    },

    /**
     * Deletes all captured events.
     */
    deleteAllSourceEntries: function() {
      this.clearEntries_();
      for (var i = 0; i < this.sourceEntryObservers_.length; ++i)
        this.sourceEntryObservers_[i].onAllSourceEntriesDeleted();
      for (var i = 0; i < this.logEntryObservers_.length; ++i)
        this.logEntryObservers_[i].onAllLogEntriesDeleted();
    },

    /**
     * Sets the value of |enableSecurityStripping_| and informs log observers
     * of the change.
     */
    setSecurityStripping: function(enableSecurityStripping) {
      this.enableSecurityStripping_ = enableSecurityStripping;
      for (var i = 0; i < this.sourceEntryObservers_.length; ++i) {
        if (this.sourceEntryObservers_[i].onSecurityStrippingChanged)
          this.sourceEntryObservers_[i].onSecurityStrippingChanged();
      }
    },

    /**
     * Returns whether or not cookies and authentication information should be
     * displayed for events that contain them.
     */
    getSecurityStripping: function() {
      return this.enableSecurityStripping_;
    },

    /**
     * Adds a listener of SourceEntries. |observer| will be called back when
     * SourceEntries are added or modified, source entries are deleted, or
     * security stripping changes:
     *
     *   observer.onSourceEntriesUpdated(sourceEntries)
     *   observer.onSourceEntriesDeleted(sourceEntryIds)
     *   ovserver.onAllSourceEntriesDeleted()
     *   observer.onSecurityStrippingChanged()
     */
    addSourceEntryObserver: function(observer) {
      this.sourceEntryObservers_.push(observer);
    },

    /**
     * Adds a listener of log entries. |observer| will be called back when new
     * log data arrives or all entries are deleted:
     *
     *   observer.onReceivedLogEntries(entries)
     *   ovserver.onAllLogEntriesDeleted()
     */
    addLogEntryObserver: function(observer) {
      this.logEntryObservers_.push(observer);
    }
  };

  return SourceTracker;
})();
