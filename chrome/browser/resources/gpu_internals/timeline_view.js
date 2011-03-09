// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview TimelineView visualizes GPU_TRACE events using the
 * gpu.Timeline component.
 */
cr.define('gpu', function() {
  function tsRound(ts) {
    return Math.round(ts * 100.0) / 100.0;
  }

  /**
   * TimelineView
   * @constructor
   * @extends {gpu.Tab}
   */
  TimelineView = cr.ui.define(gpu.Tab);

  TimelineView.prototype = {
    __proto__: gpu.Tab.prototype,

    decorate: function() {
      tracingController.addEventListener('traceBegun', this.refresh.bind(this));
      tracingController.addEventListener('traceEnded', this.refresh.bind(this));
      this.addEventListener('selectedChange', this.onViewSelectedChange_);

      this.refresh();
    },

    onViewSelectedChange_: function() {
      if (this.selected) {
        if (tracingController.traceEvents.length == 0) {
          tracingController.beginTracing();
        }
        if (this.needsRefreshOnShow_) {
          this.needsRefreshOnShow_ = false;
          this.refresh();
        }
      }
    },

    /**
     * Updates the view based on its currently known data
     */
    refresh: function() {
      if (this.parentNode.selectedTab != this) {
        this.needsRefreshOnShow_ = true;
      }

      console.log('TimelineView.refresh');
      var events = tracingController.traceEvents;
      this.timelineModel_ = new gpu.TimelineModel(events);

      // remove old timeline
      var timelineContainer = $('timeline-view-timeline-container');
      timelineContainer.textContent = '';

      // create new timeline if needed
      if (events.length) {
        this.timeline_ = new gpu.Timeline();
        this.timeline_.model = this.timelineModel_;
        timelineContainer.appendChild(this.timeline_);
        this.timeline_.onResize();
        this.timeline_.addEventListener('selectionChanging',
                                        this.onSelectionChanging_.bind(this));
        this.timeline_.addEventListener('selectionChange',
                                        this.onSelectionChanged_.bind(this));
      } else {
        this.timeline_ = null;
      }
    },
    onSelectionChanging_: function(e) {
      var text =
          'Start:    ' + tsRound(e.loWX) + 'ms\n' +
          'End:      ' + tsRound(e.hiWX) + 'ms\n' +
          'Duration: ' + tsRound(e.hiWX - e.loWX) + 'ms\n';
      var outputElem = $('timeline-selection-summary');
      outputElem.textContent = text;
    },

    onSelectionChanged_: function(e) {
      var timeline = this.timeline_;
      var selection = timeline.selection;
      if (!selection.length) {
        var outputDiv = $('timeline-selection-summary');
        outputDiv.textContent = '';
        return;
      }

      var text = '';
      var tsLo = Math.min.apply(Math, selection.map(
          function(s) {return s.slice.start;}));
      var tsHi = Math.max.apply(Math, selection.map(
          function(s) {return s.slice.end;}));

      text += 'Start:    ' + tsRound(tsLo) + 'ms<br>';
      text += 'Duration: ' + tsRound(tsHi - tsLo) + 'ms<br>';

      // compute total selection duration
      var titles = selection.map(function(i) { return i.slice.title; });
      text += '<ul>';
      var slicesByTitle = {};
      for (var i = 0; i < selection.length; i++) {
        var slice = selection[i].slice;
        if (!slicesByTitle[slice.title])
          slicesByTitle[slice.title] = {
            slices: []
          };
        slicesByTitle[slice.title].slices.push(slice);
      }
      for (var sliceGroupTitle in slicesByTitle) {
        var sliceGroup = slicesByTitle[sliceGroupTitle];
        text += '<li>' +
            sliceGroupTitle + ': ' + sliceGroup.slices.length +
            '</li>';
      }
      text += '</ul>';

      // done
      var outputDiv = $('timeline-selection-summary');
      outputDiv.innerHTML = text;
    }
  };

  return {
    TimelineView: TimelineView
  };
});
