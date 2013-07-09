// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var WaterfallRow = (function() {
  'use strict';

  /**
   * A WaterfallRow represents the row corresponding to a single SourceEntry
   * displayed by the EventsWaterfallView.
   *
   * @constructor
   */

  // TODO(viona):
  // -Support nested events.
  // -Handle updating length when an event is stalled.
  function WaterfallRow(parentView, sourceEntry, eventList) {
    this.parentView_ = parentView;
    this.sourceEntry_ = sourceEntry;

    this.eventTypes_ = eventList;
    this.description_ = sourceEntry.getDescription();

    this.createRow_();
  }

  WaterfallRow.prototype = {
    onSourceUpdated: function() {
      this.updateRow();
    },

    updateRow: function() {
      var scale = this.parentView_.getScaleFactor();
      // In some cases, the REQUEST_ALIVE event has been received, while the
      // URL Request to start the job has not been received. In that case, the
      // description obtained is incorrect. The following fixes that.
      if (this.description_ == '') {
        this.urlCell_.innerHTML = '';
        this.description_ = this.sourceEntry_.getDescription();
        addTextNode(this.urlCell_, this.description_);
      }

      this.barCell_.innerHTML = '';
      var matchingEventPairs = this.findLogEntryPairs_(this.eventTypes_);

      // Creates the spacing in the beginning to show start time.
      var startTime = this.parentView_.getStartTime();
      var sourceEntryStartTime = this.sourceEntry_.getStartTime();
      var delay = sourceEntryStartTime - startTime;
      var scaledMinTime = delay * scale;
      this.createNode_(this.barCell_, scaledMinTime, 'padding');

      var currentEnd = sourceEntryStartTime;
      for (var i = 0; i < matchingEventPairs.length; ++i) {
        var event = matchingEventPairs[i];
        var startTicks = event.startEntry.time;
        var endTicks = event.endEntry.time;
        event.eventType = event.startEntry.type;
        event.startTime = timeutil.convertTimeTicksToTime(startTicks);
        event.endTime = timeutil.convertTimeTicksToTime(endTicks);
        event.eventDuration = event.endTime - event.startTime;

        // Handles the spaces between events.
        if (currentEnd < event.startTime) {
          var eventDuration = event.startTime - currentEnd;
          var eventWidth = eventDuration * scale;
          this.createNode_(this.barCell_, eventWidth, this.type_);
        }

        // Creates event bars.
        var eventType = eventTypeToCssClass_(EventTypeNames[event.eventType]);
        var eventWidth = event.eventDuration * scale;
        this.createNode_(this.barCell_, eventWidth, eventType);
        currentEnd = event.startTime + event.eventDuration;
      }
      // Creates a bar for the part after the last event.
      if (this.getEndTime() > currentEnd) {
        var endWidth = (this.getEndTime() - currentEnd) * scale;
        this.createNode_(this.barCell_, endWidth, this.type_);
      }
    },

    getStartTime: function() {
      return this.sourceEntry_.getStartTime();
    },

    getEndTime: function() {
      return this.sourceEntry_.getEndTime();
    },

    createRow_: function() {
      // Create a row.
      var tr = addNode($(WaterfallView.TBODY_ID), 'tr');
      tr._id = this.sourceEntry_.getSourceId();
      this.row_ = tr;

      var idCell = addNode(tr, 'td');
      addTextNode(idCell, this.sourceEntry_.getSourceId());

      var urlCell = addNode(tr, 'td');
      urlCell.classList.add('waterfall-view-url-cell');
      addTextNode(urlCell, this.description_);
      this.urlCell_ = urlCell;

      // Creates the offset for where the color bar appears.
      var barCell = addNode(tr, 'td');
      barCell.classList.add('waterfall-view-row');
      this.barCell_ = barCell;

      this.updateRow();

      var sourceTypeString = this.sourceEntry_.getSourceTypeString();
      this.type_ = eventTypeToCssClass_(sourceTypeString);
    },

    // Generates nodes.
    createNode_: function(parentNode, durationScaled, eventType) {
      var newNode = addNode(parentNode, 'div');
      setNodeWidth(newNode, durationScaled);
      newNode.classList.add('waterfall-view-row-' + eventType);
      return newNode;
    },

    /**
     * Finds pairs of starting and ending events of all types that are in
     * typeList. Currently does not handle nested events. Can consider adding
     * starting events to a stack and popping off as their close events are
     * found. Returns an array of objects containing start/end entry pairs,
     * in the format {startEntry, endEntry}.
     */
    findLogEntryPairs_: function() {
      var typeList = this.eventTypes_;
      var matchingEventPairs = [];
      var startEntries = {};
      var entries = this.sourceEntry_.getLogEntries();
      for (var i = 0; i < entries.length; ++i) {
        var type = entries[i].type;
        if (typeList.indexOf(type) < 0) {
          continue;
        }
        if (entries[i].phase == EventPhase.PHASE_BEGIN) {
          startEntries[type] = entries[i];
        }
        if (startEntries[type] && entries[i].phase == EventPhase.PHASE_END) {
          var event = {
            startEntry: startEntries[type],
            endEntry: entries[i],
          };
          matchingEventPairs.push(event);
        }
      }
      return matchingEventPairs;
    },

  };

  function eventTypeToCssClass_(rawEventType) {
    return rawEventType.toLowerCase().replace(/_/g, '-');
  }

  return WaterfallRow;
})();
