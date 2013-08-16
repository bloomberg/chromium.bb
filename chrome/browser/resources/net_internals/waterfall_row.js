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

  // Offset of popup from mouse location.
  var POPUP_OFFSET_FROM_CURSOR = 25;

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
        this.sourceCell_.innerHTML = '';
        this.description_ = this.sourceEntry_.getDescription();
        addTextNode(this.sourceCell_, this.description_);
      }

      this.rowCell_.innerHTML = '';

      var matchingEventPairs = this.findLogEntryPairs_(this.eventTypes_);

      // Creates the spacing in the beginning to show start time.
      var startTime = this.parentView_.getStartTime();
      var sourceEntryStartTime = this.sourceEntry_.getStartTime();
      var delay = sourceEntryStartTime - startTime;
      var frontNode = addNode(this.rowCell_, 'div');
      frontNode.classList.add('waterfall-view-padding');
      setNodeWidth(frontNode, delay * scale);

      var barCell = addNode(this.rowCell_, 'div');
      barCell.classList.add('waterfall-view-bar');

      if (this.sourceEntry_.isError()) {
        barCell.classList.add('error');
      }

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
          var padNode = this.createNode_(
              barCell, eventDuration, this.type_, 'source');
        }

        // Creates event bars.
        var eventType = eventTypeToCssClass_(EventTypeNames[event.eventType]);
        var eventNode = this.createNode_(
            barCell, event.eventDuration, eventType, event);
        currentEnd = event.startTime + event.eventDuration;
      }

      // Creates a bar for the part after the last event.
      if (this.getEndTime() > currentEnd) {
        var endDuration = (this.getEndTime() - currentEnd);
        var endNode = this.createNode_(
            barCell, endDuration, this.type_, 'source');
      }
    },

    getStartTime: function() {
      return this.sourceEntry_.getStartTime();
    },

    getEndTime: function() {
      return this.sourceEntry_.getEndTime();
    },

    clearPopup_: function(parentNode) {
      parentNode.innerHTML = '';
    },

    createPopup_: function(parentNode, event, eventType, duration, mouse) {
      var tableStart = this.parentView_.getStartTime();

      var newPopup = addNode(parentNode, 'div');
      newPopup.classList.add('waterfall-view-popup');

      var popupList = addNode(newPopup, 'ul');
      popupList.classList.add('waterfall-view-popup-list');

      popupList.style.maxWidth =
          $(WaterfallView.MAIN_BOX_ID).offsetWidth * 0.5 + 'px';

      this.createPopupItem_(
          popupList, 'Has Error', this.sourceEntry_.isError());

      this.createPopupItem_(
          popupList, 'Event Type', eventType);
      this.createPopupItem_(
          popupList, 'Event Duration', duration.toFixed(0) + 'ms');

      if (event != 'source') {
        this.createPopupItem_(
            popupList, 'Event Start Time', event.startTime - tableStart + 'ms');
        this.createPopupItem_(
            popupList, 'Event End Time', event.endTime - tableStart + 'ms');
      }

      this.createPopupItem_(
          popupList, 'Source Start Time',
          this.getStartTime() - tableStart + 'ms');
      this.createPopupItem_(
          popupList, 'Source End Time', this.getEndTime() - tableStart + 'ms');
      this.createPopupItem_(
          popupList, 'Source ID', this.sourceEntry_.getSourceId());
      var urlListItem = this.createPopupItem_(
          popupList, 'Source Description', this.description_);

      urlListItem.classList.add('waterfall-view-popup-list-url-item');

      // Fixes cases where the popup appears 'off-screen'.
      var popupLeft = mouse.pageX - newPopup.offsetWidth;
      if (popupLeft < 0) {
        popupLeft = mouse.pageX;
      }
      newPopup.style.left = popupLeft +
          $(WaterfallView.MAIN_BOX_ID).scrollLeft -
          $(WaterfallView.BAR_TABLE_ID).offsetLeft + 'px';

      var popupTop = mouse.pageY - newPopup.offsetHeight -
          POPUP_OFFSET_FROM_CURSOR;
      if (popupTop < 0) {
        popupTop = mouse.pageY;
      }
      newPopup.style.top = popupTop +
          $(WaterfallView.MAIN_BOX_ID).scrollTop -
          $(WaterfallView.BAR_TABLE_ID).offsetTop + 'px';
    },

    createPopupItem_: function(parentPopup, key, popupInformation) {
      var popupItem = addNode(parentPopup, 'li');
      addTextNode(popupItem, key + ': ' + popupInformation);
      return popupItem;
    },

    createRow_: function() {
      // Create a row.
      var tr = addNode($(WaterfallView.BAR_TBODY_ID), 'tr');
      tr.classList.add('waterfall-view-table-row');

      // Creates the color bar.

      var rowCell = addNode(tr, 'td');
      rowCell.classList.add('waterfall-view-row');
      this.rowCell_ = rowCell;

      var sourceTypeString = this.sourceEntry_.getSourceTypeString();
      this.type_ = eventTypeToCssClass_(sourceTypeString);

      var infoTr = addNode($(WaterfallView.INFO_TBODY_ID), 'tr');
      infoTr.classList.add('waterfall-view-information-row');

      var idCell = addNode(infoTr, 'td');
      idCell.classList.add('waterfall-view-id-cell');
      var idValue = this.sourceEntry_.getSourceId();
      var idLink = addNodeWithText(idCell, 'a', idValue);
      idLink.href = '#events&s=' + idValue;

      var sourceCell = addNode(infoTr, 'td');
      sourceCell.classList.add('waterfall-view-url-cell');
      addTextNode(sourceCell, this.description_);
      this.sourceCell_ = sourceCell;

      this.updateRow();
    },

    // Generates nodes.
    createNode_: function(parentNode, duration, eventType, event) {
      var linkNode = addNode(parentNode, 'a');
      linkNode.href = '#events&s=' + this.sourceEntry_.getSourceId();

      var scale = this.parentView_.getScaleFactor();
      var newNode = addNode(linkNode, 'div');
      setNodeWidth(newNode, duration * scale);
      newNode.classList.add(eventType);
      newNode.classList.add('waterfall-view-bar-component');
      newNode.addEventListener(
          'mouseover',
          this.createPopup_.bind(this, newNode, event, eventType, duration),
          true);
      newNode.addEventListener(
          'mouseout', this.clearPopup_.bind(this, newNode), true);
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
        var currentEntry = entries[i];
        var type = currentEntry.type;
        if (typeList.indexOf(type) < 0) {
          continue;
        }
        if (currentEntry.phase == EventPhase.PHASE_BEGIN) {
          startEntries[type] = currentEntry;
        }
        if (startEntries[type] && currentEntry.phase == EventPhase.PHASE_END) {
          var event = {
            startEntry: startEntries[type],
            endEntry: currentEntry,
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
