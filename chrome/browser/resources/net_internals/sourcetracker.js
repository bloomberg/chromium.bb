// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class keeps track of all NetLog events.
 * It receives events from the browser and when loading a log file, and passes
 * them on to all its observers.
 *
 * @constructor
 */
function SourceTracker() {
  this.observers_ = [];

  // True when cookies and authentication information should be removed from
  // displayed events.  When true, such information should be hidden from
  // all pages.
  this.enableSecurityStripping_ = true;

  this.clearEntries_();
}

/**
 * Clears all log entries and SourceEntries and related state.
 */
SourceTracker.prototype.clearEntries_ = function() {
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
};

/**
 * Returns a list of all captured events.
 */
SourceTracker.prototype.getAllCapturedEvents = function() {
  return this.capturedEvents_;
};

/**
 * Returns a list of all SourceEntries.
 */
SourceTracker.prototype.getAllSourceEntries = function() {
  return this.sourceEntries_;
};

/**
 * Returns the number of events that were captured while we were
 * listening for events.
 */
SourceTracker.prototype.getNumActivelyCapturedEvents = function() {
  return this.capturedEvents_.length - this.numPassivelyCapturedEvents_;
};

/**
 * Returns the number of events that were captured passively by the
 * browser prior to when the net-internals page was started.
 */
SourceTracker.prototype.getNumPassivelyCapturedEvents = function() {
  return this.numPassivelyCapturedEvents_;
};

/**
 * Returns the specified SourceEntry.
 */
SourceTracker.prototype.getSourceEntry = function(id) {
  return this.sourceEntries_[id];
};

SourceTracker.prototype.onReceivedPassiveLogEntries = function(entries) {
  // Due to an expected race condition, it is possible to receive actively
  // captured log entries before the passively logged entries are received.
  //
  // When that happens, we create a copy of the actively logged entries, delete
  // all entries, and, after handling all the passively logged entries, add back
  // the deleted actively logged entries.
  var earlyActivelyCapturedEvents = this.capturedEvents_.slice(0);
  if (earlyActivelyCapturedEvents.length > 0)
    this.deleteAllSourceEntries();

  this.numPassivelyCapturedEvents_ = entries.length;
  for (var i = 0; i < entries.length; ++i)
    entries[i].wasPassivelyCaptured = true;
  this.onReceivedLogEntries(entries);

  // Add back early actively captured events, if any.
  if (earlyActivelyCapturedEvents.length)
    this.onReceivedLogEntries(earlyActivelyCapturedEvents);
};

/**
 * Sends each entry to all log observers, and updates |capturedEvents_|.
 * Also assigns unique ids to log entries without a source.
 */
SourceTracker.prototype.onReceivedLogEntries = function(logEntries) {
  for (var e = 0; e < logEntries.length; ++e) {
    var logEntry = logEntries[e];

    // Assign unique ID, if needed.
    if (logEntry.source.id == 0) {
      logEntry.source.id = this.nextSourcelessEventId_;
      --this.nextSourcelessEventId_;
    } else if (this.maxReceivedSourceId_ < logEntry.source.id) {
      this.maxReceivedSourceId_ = logEntry.source.id;
    }

    var sourceEntry = this.sourceEntries_[logEntry.source.id];
    if (!sourceEntry) {
      sourceEntry = new SourceEntry(logEntry, this.maxReceivedSourceId_);
      this.sourceEntries_[logEntry.source.id] = sourceEntry;
    } else {
      sourceEntry.update(logEntry);
    }
    this.capturedEvents_.push(logEntry);

    // TODO(mmenke):  Send a list of all updated source entries instead,
    //                eliminating duplicates, to reduce CPU usage.
    for (var i = 0; i < this.observers_.length; ++i)
      this.observers_[i].onSourceEntryUpdated(sourceEntry);
  }
};

/**
 * Deletes captured events with source IDs in |sourceEntryIds|.
 */
SourceTracker.prototype.deleteSourceEntries = function(sourceEntryIds) {
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

  for (var i = 0; i < this.observers_.length; ++i)
    this.observers_[i].onSourceEntriesDeleted(sourceEntryIds);
};

/**
 * Deletes all captured events.
 */
SourceTracker.prototype.deleteAllSourceEntries = function() {
  this.clearEntries_();
  for (var i = 0; i < this.observers_.length; ++i)
    this.observers_[i].onAllSourceEntriesDeleted();
};

/**
 * Sets the value of |enableSecurityStripping_| and informs log observers
 * of the change.
 */
SourceTracker.prototype.setSecurityStripping =
    function(enableSecurityStripping) {
  this.enableSecurityStripping_ = enableSecurityStripping;
  for (var i = 0; i < this.observers_.length; ++i) {
    if (this.observers_[i].onSecurityStrippingChanged)
      this.observers_[i].onSecurityStrippingChanged();
  }
};

/**
 * Returns whether or not cookies and authentication information should be
 * displayed for events that contain them.
 */
SourceTracker.prototype.getSecurityStripping = function() {
  return this.enableSecurityStripping_;
};

/**
 * Adds a listener of log entries. |observer| will be called back when new log
 * data arrives, source entries are deleted, or security stripping changes
 * through:
 *
 *   observer.onSourceEntryUpdated(sourceEntry)
 *   observer.deleteSourceEntries(sourceEntryIds)
 *   ovserver.deleteAllSourceEntries()
 *   observer.onSecurityStrippingChanged()
 */
SourceTracker.prototype.addObserver = function(observer) {
  this.observers_.push(observer);
};
