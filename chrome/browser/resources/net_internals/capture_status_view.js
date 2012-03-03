// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The status view at the top of the page when in capturing mode.
 */
var CaptureStatusView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  function CaptureStatusView() {
    superClass.call(this, CaptureStatusView.MAIN_BOX_ID);

    $(CaptureStatusView.STOP_BUTTON_ID).onclick = switchToViewOnlyMode_;

    $(CaptureStatusView.RESET_BUTTON_ID).onclick =
        EventsTracker.getInstance().deleteAllLogEntries.bind(
            EventsTracker.getInstance());
    $(CaptureStatusView.CLEAR_CACHE_BUTTON_ID).onclick =
        g_browser.sendClearAllCache.bind(g_browser);
    $(CaptureStatusView.FLUSH_SOCKETS_BUTTON_ID).onclick =
        g_browser.sendFlushSocketPools.bind(g_browser);

    $(CaptureStatusView.TOGGLE_EXTRAS_ID).onclick =
        this.toggleExtras_.bind(this);

    this.capturedEventsCountBox_ =
        $(CaptureStatusView.CAPTURED_EVENTS_COUNT_ID);
    this.updateEventCounts_();

    EventsTracker.getInstance().addLogEntryObserver(this);
  }

  // IDs for special HTML elements in status_view.html
  CaptureStatusView.MAIN_BOX_ID = 'capture-status-view';
  CaptureStatusView.STOP_BUTTON_ID = 'capture-status-view-stop';
  CaptureStatusView.RESET_BUTTON_ID = 'capture-status-view-reset';
  CaptureStatusView.CLEAR_CACHE_BUTTON_ID = 'capture-status-view-clear-cache';
  CaptureStatusView.FLUSH_SOCKETS_BUTTON_ID =
      'capture-status-view-flush-sockets';
  CaptureStatusView.CAPTURED_EVENTS_COUNT_ID =
      'capture-status-view-captured-events-count';
  CaptureStatusView.TOGGLE_EXTRAS_ID = 'capture-status-view-toggle-extras';
  CaptureStatusView.EXTRAS_ID = 'capture-status-view-extras';

  CaptureStatusView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called whenever new log entries have been received.
     */
    onReceivedLogEntries: function(logEntries) {
      this.updateEventCounts_();
    },

    /**
     * Called whenever all log events are deleted.
     */
    onAllLogEntriesDeleted: function() {
      this.updateEventCounts_();
    },

    /**
     * Updates the counters showing how many events have been captured.
     */
    updateEventCounts_: function() {
      this.capturedEventsCountBox_.textContent =
          EventsTracker.getInstance().getNumCapturedEvents();
    },

    /**
     * Toggles the visibility of the "extras" action bar.
     */
    toggleExtras_: function() {
      var toggle = $(CaptureStatusView.TOGGLE_EXTRAS_ID);
      var extras = $(CaptureStatusView.EXTRAS_ID);

      var isVisible = extras.style.display == '';

      // Toggle between the left-facing triangle and right-facing triange.
      toggle.className = isVisible ?
          'capture-status-view-rotateleft' : 'capture-status-view-rotateright';
      setNodeDisplay(extras, !isVisible);
    }
  };

  /**
   * Calls the corresponding function of MainView.  This is needed because we
   * can't call MainView.getInstance() in the constructor, as it hasn't been
   * created yet.
   */
  function switchToViewOnlyMode_() {
    MainView.getInstance().switchToViewOnlyMode();
  }

  return CaptureStatusView;
})();
