// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays controls for capturing network events.
 *  @constructor
 */
function CaptureView() {
  const mainBoxId = 'capture-view-tab-content';
  const byteLoggingCheckboxId = 'capture-view-byte-logging-checkbox';
  const passivelyCapturedCountId = 'capture-view-passively-captured-count';
  const activelyCapturedCountId = 'capture-view-actively-captured-count';
  const deleteAllId = 'capture-view-delete-all';
  const tipAnchorId = 'capture-view-tip-anchor';
  const tipDivId = 'capture-view-tip-div';

  DivView.call(this, mainBoxId);

  var byteLoggingCheckbox = $(byteLoggingCheckboxId);
  byteLoggingCheckbox.onclick =
      this.onSetByteLogging_.bind(this, byteLoggingCheckbox);

  this.activelyCapturedCountBox_ = $(activelyCapturedCountId);
  this.passivelyCapturedCountBox_ = $(passivelyCapturedCountId);
  $(deleteAllId).onclick = g_browser.sourceTracker.deleteAllSourceEntries.bind(
      g_browser.sourceTracker);

  $(tipAnchorId).onclick =
      this.toggleCommandLineTip_.bind(this, tipDivId);

  this.updateEventCounts_();

  g_browser.sourceTracker.addObserver(this);
}

inherits(CaptureView, DivView);

/**
 * Called whenever a new event is received.
 */
CaptureView.prototype.onSourceEntryUpdated = function(sourceEntry) {
  this.updateEventCounts_();
};

/**
 * Toggles the visilibity on the command-line tip.
 */
CaptureView.prototype.toggleCommandLineTip_ = function(divId) {
  var n = $(divId);
  var isVisible = n.style.display != 'none';
  setNodeDisplay(n, !isVisible);
  return false;  // Prevent default handling of the click.
};

/**
 * Called whenever some log events are deleted.  |sourceIds| lists
 * the source IDs of all deleted log entries.
 */
CaptureView.prototype.onSourceEntriesDeleted = function(sourceIds) {
  this.updateEventCounts_();
};

/**
 * Called whenever all log events are deleted.
 */
CaptureView.prototype.onAllSourceEntriesDeleted = function() {
  this.updateEventCounts_();
};

/**
 * Called when a log file is loaded, after clearing the old log entries and
 * loading the new ones.  Returns false to indicate the view should
 * be hidden.
 */
CaptureView.prototype.onLoadLogFinish = function(data) {
  return false;
};

/**
 * Updates the counters showing how many events have been captured.
 */
CaptureView.prototype.updateEventCounts_ = function() {
  this.activelyCapturedCountBox_.textContent =
      g_browser.sourceTracker.getNumActivelyCapturedEvents();
  this.passivelyCapturedCountBox_.textContent =
      g_browser.sourceTracker.getNumPassivelyCapturedEvents();
};

/**
 * Depending on the value of the checkbox, enables or disables logging of
 * actual bytes transferred.
 */
CaptureView.prototype.onSetByteLogging_ = function(byteLoggingCheckbox) {
  if (byteLoggingCheckbox.checked) {
    g_browser.setLogLevel(LogLevelType.LOG_ALL);
  } else {
    g_browser.setLogLevel(LogLevelType.LOG_ALL_BUT_BYTES);
  }
};

