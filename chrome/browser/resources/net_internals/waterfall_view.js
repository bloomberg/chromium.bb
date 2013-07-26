// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(viona): Write a README document/instructions.

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
        'click', this.setStartEndTimes_.bind(this), true);

    $(WaterfallView.MAIN_BOX_ID).addEventListener(
        'mousewheel', this.scrollToZoom_.bind(this), true);

    this.initializeSourceList_();
  }

  WaterfallView.TAB_ID = 'tab-handle-waterfall';
  WaterfallView.TAB_NAME = 'Waterfall';
  WaterfallView.TAB_HASH = '#waterfall';

  // IDs for special HTML elements in events_waterfall_view.html.
  WaterfallView.MAIN_BOX_ID = 'waterfall-view-tab-content';
  WaterfallView.TBODY_ID = 'waterfall-view-source-list-tbody';
  WaterfallView.SCALE_ID = 'waterfall-view-adjust-to-window';
  WaterfallView.ID_HEADER_ID = 'waterfall-view-source-id';
  WaterfallView.SOURCE_HEADER_ID = 'waterfall-view-source-header';
  WaterfallView.TIME_SCALE_HEADER_ID = 'waterfall-view-time-scale-labels';
  WaterfallView.TIME_RANGE_ID = 'waterfall-view-time-range-submit';
  WaterfallView.START_TIME_ID = 'waterfall-view-start-input';
  WaterfallView.END_TIME_ID = 'waterfall-view-end-input';

  // The number of units mouse wheel deltas increase for each tick of the
  // wheel.
  var MOUSE_WHEEL_UNITS_PER_CLICK = 120;

  // Amount we zoom for one vertical tick of the mouse wheel, as a ratio.
  var MOUSE_WHEEL_ZOOM_RATE = 1.25;
  // Amount we scroll for one horizontal tick of the mouse wheel, in pixels.
  var MOUSE_WHEEL_SCROLL_RATE = MOUSE_WHEEL_UNITS_PER_CLICK;

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
        this.eventsList_ = this.createEventsList_();
      }
      for (var i = 0; i < sourceEntries.length; ++i) {
        var sourceEntry = sourceEntries[i];
        var id = sourceEntry.getSourceId();
        for (var j = 0; j < this.eventsList_.length; ++j) {
          var eventObject = this.eventsList_[j];
          if (sourceEntry.getSourceType() == eventObject.mainEvent) {
            var row = this.sourceIdToRowMap_[id];
            if (!row) {
              var newRow = new WaterfallRow(
                  this, sourceEntry, eventObject.importantEventTypes);
              if (newRow == undefined) {
                console.log(sourceEntry);
              }
              this.sourceIdToRowMap_[id] = newRow;
            } else {
              row.onSourceUpdated();
            }
          }
        }
      }
      this.updateTimeScale_(this.scaleFactor_);
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
     * Changes scroll position of the window such that horizontally, everything
     * within the specified range fits into the user's viewport.
     * TODO(viona): Find a way to get rid of the magic number.
     */
    adjustToWindow_: function(windowStart, windowEnd) {
      var maxWidth = $(WaterfallView.MAIN_BOX_ID).offsetWidth - 50;
      $(WaterfallView.TBODY_ID).width = maxWidth + 'px';
      var totalDuration = 0;
      if (windowEnd != -1) {
        totalDuration = windowEnd - windowStart;
      } else {
        for (var id in this.sourceIdToRowMap_) {
          var row = this.sourceIdToRowMap_[id];
          var rowDuration = row.getEndTime() - this.startTime_;
          if (totalDuration < rowDuration && !row.hide) {
            totalDuration = rowDuration;
          }
        }
      }
      if (totalDuration <= 0) {
        return;
      }
      this.scaleAll_(maxWidth / totalDuration);
      var waterfallLeft = $(WaterfallView.TIME_SCALE_HEADER_ID).offsetLeft;
      $(WaterfallView.MAIN_BOX_ID).scrollLeft =
          windowStart * this.scaleFactor_ + waterfallLeft;
    },

    // Updates the time tick indicators.
    updateTimeScale_: function(scaleFactor) {
      var timePerTick = 1;
      var minTickDistance = 20;

      $(WaterfallView.TIME_SCALE_HEADER_ID).innerHTML = '';

      // Holder provides environment to prevent wrapping.
      var timeTickRow = addNode($(WaterfallView.TIME_SCALE_HEADER_ID), 'div');
      timeTickRow.classList.add('waterfall-view-time-scale-row');

      var availableWidth = $(WaterfallView.TBODY_ID).clientWidth;
      var tickDistance = scaleFactor * timePerTick;

      while (tickDistance < minTickDistance) {
        timePerTick = timePerTick * 10;
        tickDistance = scaleFactor * timePerTick;
      }

      var tickCount = availableWidth / tickDistance;
      for (var i = 0; i < tickCount; ++i) {
        var timeCell = addNode(timeTickRow, 'div');
        setNodeWidth(timeCell, tickDistance);
        timeCell.classList.add('waterfall-view-time-scale');
        timeCell.title = i * timePerTick + ' to ' +
           (i + 1) * timePerTick + ' ms';
        // Red marker for every 5th bar.
        if (i % 5 == 0) {
          timeCell.classList.add('waterfall-view-time-scale-special');
        }
      }
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
      this.updateTimeScale_(scaleFactor);
    },

    scrollToZoom_: function(event) {
      // To use scrolling to control zoom, hold down the alt key and scroll.
      if ('wheelDelta' in event && event.altKey) {
        var zoomFactor = Math.pow(MOUSE_WHEEL_ZOOM_RATE,
             event.wheelDeltaY / MOUSE_WHEEL_UNITS_PER_CLICK);

        var waterfallLeft = $(WaterfallView.TIME_SCALE_HEADER_ID).offsetLeft;
        var oldCursorPosition = event.pageX +
            $(WaterfallView.MAIN_BOX_ID).scrollLeft;
        var oldCursorPositionInTable = oldCursorPosition - waterfallLeft;

        this.scaleAll_(this.scaleFactor_ * zoomFactor);

        // Shifts the view when scrolling. newScroll could be less than 0 or
        // more than the maximum scroll position, but both cases are handled
        // by the inbuilt scrollLeft implementation.
        var newScroll =
            oldCursorPositionInTable * zoomFactor - event.pageX + waterfallLeft;
        $(WaterfallView.MAIN_BOX_ID).scrollLeft = newScroll;
      }
    },

    // Parses user input, then calls adjustToWindow to shift that into view.
    setStartEndTimes_: function() {
      var windowStart = parseInt($(WaterfallView.START_TIME_ID).value);
      var windowEnd = parseInt($(WaterfallView.END_TIME_ID).value);
      if ($(WaterfallView.END_TIME_ID).value == '') {
        windowEnd = -1;
      }
      if ($(WaterfallView.START_TIME_ID).value == '') {
        windowStart = 0;
      }
      this.adjustToWindow_(windowStart, windowEnd);
    },

    // Provides a structure that can be used to define source entry types and
    // the log entries that are of interest.
    createEventsList_: function() {
      var eventsList = [];
      // Creating list of events.
      // TODO(viona): This is hard-coded. Consider user-input.
      //              Also, work on getting socket information inlined in the
      //              relevant URL_REQUEST events.
      var urlRequestEvents = [
        EventType.HTTP_STREAM_REQUEST,
        EventType.HTTP_STREAM_REQUEST_BOUND_TO_JOB,
        EventType.HTTP_TRANSACTION_READ_HEADERS
      ];

      eventsList.push(this.createEventTypeObjects_(
          EventSourceType.URL_REQUEST, urlRequestEvents));

      var httpStreamJobEvents = [EventType.PROXY_SERVICE];
      eventsList.push(this.createEventTypeObjects_(
          EventSourceType.HTTP_STREAM_JOB, httpStreamJobEvents));

      return eventsList;
    },

    // Creates objects that can be used to define source entry types and
    // the log entries that are of interest.
    createEventTypeObjects_: function(mainEventType, eventTypesList) {
      var eventTypeObject = {
        mainEvent: mainEventType,
        importantEventTypes: eventTypesList
      };
      return eventTypeObject;
    },
  };

  return WaterfallView;
})();
