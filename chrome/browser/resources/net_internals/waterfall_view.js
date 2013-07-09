// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** This view displays the event waterfall. */
var WaterfallView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function WaterfallView() {
    assertFirstConstructorCall(WaterfallView);

    // Call superclass's constructor.
    superClass.call(this, WaterfallView.MAIN_BOX_ID);

    SourceTracker.getInstance().addSourceEntryObserver(this);

    // For adjusting the range of view.
    $(WaterfallView.SCALE_ID).addEventListener(
        'click', this.adjustToWindow_.bind(this), true);

    this.initializeSourceList_();
  }

  WaterfallView.TAB_ID = 'tab-handle-waterfall';
  WaterfallView.TAB_NAME = 'Waterfall';
  WaterfallView.TAB_HASH = '#waterfall';

  // IDs for special HTML elements in events_waterfall_view.html.
  WaterfallView.MAIN_BOX_ID = 'waterfall-view-tab-content';
  WaterfallView.TBODY_ID = 'waterfall-view-source-list-tbody';
  WaterfallView.SCALE_ID = 'waterfall-view-time-scale';
  WaterfallView.ID_HEADER_ID = 'waterfall-view-source-id';
  WaterfallView.SOURCE_HEADER_ID = 'waterfall-view-source-header';

  cr.addSingletonGetter(WaterfallView);

  WaterfallView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Creates new WaterfallRows for URL Requests when the sourceEntries are
     * updated if they do not already exist.
     * Updates pre-existing WaterfallRows that correspond to updated sources.
     */
    onSourceEntriesUpdated: function(sourceEntries) {
      if (this.startTime_ == null && sourceEntries.length > 0) {
        var logEntries = sourceEntries[0].getLogEntries();
        this.startTime_ = timeutil.convertTimeTicksToTime(logEntries[0].time);
        // Initial scale factor.
        this.scaleFactor_ = 0.1;
      }
      for (var i = 0; i < sourceEntries.length; ++i) {
        var sourceEntry = sourceEntries[i];
        var id = sourceEntry.getSourceId();
        if (sourceEntry.getSourceType() == EventSourceType.URL_REQUEST) {
          var row = this.sourceIdToRowMap_[id];
          if (!row) {
            var importantEventTypes = [
                EventType.HTTP_STREAM_REQUEST,
                EventType.HTTP_TRANSACTION_READ_HEADERS
            ];
            this.sourceIdToRowMap_[id] =
                new WaterfallRow(this, sourceEntry, importantEventTypes);
          } else {
            row.onSourceUpdated();
          }
        }
      }
    },

    onAllSourceEntriesDeleted: function() {
      this.initializeSourceList_();
    },

    onLoadLogFinish: function(data) {
      return true;
    },

    getScaleFactor: function() {
      return this.scaleFactor_;
    },

    getStartTime: function() {
      return this.startTime_;
    },

    /**
     * Initializes the list of source entries.  If source entries are already
     * being displayed, removes them all in the process.
     */
    initializeSourceList_: function() {
      this.sourceIdToRowMap_ = {};
      $(WaterfallView.TBODY_ID).innerHTML = '';
      this.startTime_ = null;
      this.scaleFactor_ = null;
    },

    /**
     * Changes width of the bars such that horizontally, everything fits into
     * the user's current window size.
     * TODO(viona): Deal with the magic number.
     */
    adjustToWindow_: function() {
      var usedWidth = $(WaterfallView.SOURCE_HEADER_ID).offsetWidth +
          $(WaterfallView.ID_HEADER_ID).offsetWidth;
      var availableWidth = ($(WaterfallView.MAIN_BOX_ID).offsetWidth -
          usedWidth - 50);
      if (availableWidth <= 0) {
        availableWidth = 1;
      }
      var totalDuration = 0;
      for (var id in this.sourceIdToRowMap_) {
        var row = this.sourceIdToRowMap_[id];
        var rowDuration = row.getEndTime() - this.startTime_;
        if (totalDuration < rowDuration) {
          totalDuration = rowDuration;
        }
      }
      var scaleFactor = availableWidth / totalDuration;
      this.scaleAll_(scaleFactor);
    },

    /**
     * Scales all existing rows by scaleFactor.
     */
    scaleAll_: function(scaleFactor) {
      this.scaleFactor_ = scaleFactor;
      for (var id in this.sourceIdToRowMap_) {
        var row = this.sourceIdToRowMap_[id];
        row.updateRow();
      }
    },
  };

  return WaterfallView;
})();
