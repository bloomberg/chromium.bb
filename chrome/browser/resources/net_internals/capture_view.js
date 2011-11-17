// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays controls for capturing network events.
 */
var CaptureView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function CaptureView() {
    assertFirstConstructorCall(CaptureView);

    // Call superclass's constructor.
    superClass.call(this, CaptureView.MAIN_BOX_ID);

    var byteLoggingCheckbox = $(CaptureView.BYTE_LOGGING_CHECKBOX_ID);
    byteLoggingCheckbox.onclick =
        this.onSetByteLogging_.bind(this, byteLoggingCheckbox);

    this.activelyCapturedCountBox_ = $(CaptureView.ACTIVELY_CAPTURED_COUNT_ID);
    this.passivelyCapturedCountBox_ =
        $(CaptureView.PASSIVELY_CAPTURED_COUNT_ID);
    $(CaptureView.DELETE_ALL_ID).onclick =
        g_browser.sourceTracker.deleteAllSourceEntries.bind(
            g_browser.sourceTracker);

    $(CaptureView.TIP_ANCHOR_ID).onclick =
        this.toggleCommandLineTip_.bind(this, CaptureView.TIP_DIV_ID);

    this.updateEventCounts_();

    g_browser.sourceTracker.addSourceEntryObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  CaptureView.TAB_HANDLE_ID = 'tab-handle-capture';

  // IDs for special HTML elements in capture_view.html
  CaptureView.MAIN_BOX_ID = 'capture-view-tab-content';
  CaptureView.BYTE_LOGGING_CHECKBOX_ID = 'capture-view-byte-logging-checkbox';
  CaptureView.PASSIVELY_CAPTURED_COUNT_ID =
      'capture-view-passively-captured-count';
  CaptureView.ACTIVELY_CAPTURED_COUNT_ID =
      'capture-view-actively-captured-count';
  CaptureView.DELETE_ALL_ID = 'capture-view-delete-all';
  CaptureView.TIP_ANCHOR_ID = 'capture-view-tip-anchor';
  CaptureView.TIP_DIV_ID = 'capture-view-tip-div';

  cr.addSingletonGetter(CaptureView);

  CaptureView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called whenever a new event is received.
     */
    onSourceEntriesUpdated: function(sourceEntries) {
      this.updateEventCounts_();
    },

    /**
     * Toggles the visilibity on the command-line tip.
     */
    toggleCommandLineTip_: function(divId) {
      var n = $(divId);
      var isVisible = n.style.display != 'none';
      setNodeDisplay(n, !isVisible);
      return false;  // Prevent default handling of the click.
    },

    /**
     * Called whenever some log events are deleted.  |sourceIds| lists
     * the source IDs of all deleted log entries.
     */
    onSourceEntriesDeleted: function(sourceIds) {
      this.updateEventCounts_();
    },

    /**
     * Called whenever all log events are deleted.
     */
    onAllSourceEntriesDeleted: function() {
      this.updateEventCounts_();
    },

    /**
     * Called when a log file is loaded, after clearing the old log entries and
     * loading the new ones.  Returns false to indicate the view should
     * be hidden.
     */
    onLoadLogFinish: function(data) {
      return false;
    },

    /**
     * Updates the counters showing how many events have been captured.
     */
    updateEventCounts_: function() {
      this.activelyCapturedCountBox_.textContent =
          g_browser.sourceTracker.getNumActivelyCapturedEvents();
      this.passivelyCapturedCountBox_.textContent =
          g_browser.sourceTracker.getNumPassivelyCapturedEvents();
    },

    /**
     * Depending on the value of the checkbox, enables or disables logging of
     * actual bytes transferred.
     */
    onSetByteLogging_: function(byteLoggingCheckbox) {
      if (byteLoggingCheckbox.checked) {
        g_browser.setLogLevel(LogLevelType.LOG_ALL);
      } else {
        g_browser.setLogLevel(LogLevelType.LOG_ALL_BUT_BYTES);
      }
    }
  };

  return CaptureView;
})();
