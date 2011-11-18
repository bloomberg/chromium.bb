// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview TimelineView visualizes TRACE_EVENT events using the
 * tracing.Timeline component.
 */
cr.define('tracing', function() {
  function tsRound(ts) {
    return Math.round(ts * 1000.0) / 1000.0;
  }
  function getPadding(text, width) {
    width = width || 0;

    if (typeof text != 'string')
      text = String(text);

    if (text.length >= width)
      return '';

    var pad = '';
    for (var i = 0; i < width - text.length; i++)
      pad += ' ';
    return pad;
  }

  function leftAlign(text, width) {
    return text + getPadding(text, width);
  }

  function rightAlign(text, width) {
    return getPadding(text, width) + text;
  }

  /**
   * TimelineView
   * @constructor
   * @extends {HTMLDivElement}
   */
  TimelineView = cr.ui.define('div');

  TimelineView.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'timeline-view';

      this.timelineContainer_ = document.createElement('div');
      this.timelineContainer_.className = 'timeline-container';

      var summaryContainer_ = document.createElement('div');
      summaryContainer_.className = 'summary-container';

      this.summaryEl_ = document.createElement('pre');
      this.summaryEl_.className = 'summary';

      summaryContainer_.appendChild(this.summaryEl_);
      this.appendChild(this.timelineContainer_);
      this.appendChild(summaryContainer_);

      this.onSelectionChangedBoundToThis_ = this.onSelectionChanged_.bind(this);
    },

    set traceEvents(traceEvents) {
      this.timelineModel_ = new tracing.TimelineModel(traceEvents);

      // remove old timeline
      this.timelineContainer_.textContent = '';

      // create new timeline if needed
      if (traceEvents.length) {
        if (this.timeline_)
          this.timeline_.detach();
        this.timeline_ = new tracing.Timeline();
        this.timeline_.model = this.timelineModel_;
        this.timeline_.focusElement = this.parentElement;
        this.timelineContainer_.appendChild(this.timeline_);
        this.timeline_.addEventListener('selectionChange',
                                        this.onSelectionChangedBoundToThis_);
        this.onSelectionChanged_();
      } else {
        this.timeline_ = null;
      }
    },

    onSelectionChanged_: function(e) {
      var timeline = this.timeline_;
      var selection = timeline.selection;
      if (!selection.length) {
        var oldScrollTop = this.timelineContainer_.scrollTop;
        this.summaryEl_.textContent = timeline.keyHelp;
        this.timelineContainer_.scrollTop = oldScrollTop;
        return;
      }

      var text = '';
      if (selection.length == 1) {
        var c0Width = 14;
        var slice = selection[0].slice;
        text = 'Selected item:\n';
        text += leftAlign('Title', c0Width) + ': ' + slice.title + '\n';
        text += leftAlign('Start', c0Width) + ': ' +
            tsRound(slice.start) + ' ms\n';
        text += leftAlign('Duration', c0Width) + ': ' +
            tsRound(slice.duration) + ' ms\n';
        if (slice.durationInUserTime)
          text += leftAlign('Duration (U)', c0Width) + ': ' +
              tsRound(slice.durationInUserTime) + ' ms\n';

        var n = 0;
        for (var argName in slice.args) {
          n += 1;
        }
        if (n > 0) {
          text += leftAlign('Args', c0Width) + ':\n';
          for (var argName in slice.args) {
            var argVal = slice.args[argName];
            text += leftAlign(' ' + argName, c0Width) + ': ' + argVal + '\n';
          }
        }
      } else {
        var c0Width = 55;
        var c1Width = 12;
        var c2Width = 5;
        text = 'Selection summary:\n';
        var tsLo = Math.min.apply(Math, selection.map(
            function(s) {return s.slice.start;}));
        var tsHi = Math.max.apply(Math, selection.map(
            function(s) {return s.slice.end;}));

        // compute total selection duration
        var titles = selection.map(function(i) { return i.slice.title; });

        var slicesByTitle = {};
        for (var i = 0; i < selection.length; i++) {
          var slice = selection[i].slice;
          if (!slicesByTitle[slice.title])
            slicesByTitle[slice.title] = {
              slices: []
            };
          slicesByTitle[slice.title].slices.push(slice);
        }
        var totalDuration = 0;
        for (var sliceGroupTitle in slicesByTitle) {
          var sliceGroup = slicesByTitle[sliceGroupTitle];
          var duration = 0;
          for (i = 0; i < sliceGroup.slices.length; i++)
            duration += sliceGroup.slices[i].duration;
          totalDuration += duration;

          text += ' ' +
              leftAlign(sliceGroupTitle, c0Width) + ': ' +
              rightAlign(tsRound(duration) + 'ms', c1Width) + '   ' +
              rightAlign(String(sliceGroup.slices.length), c2Width) +
              ' occurrences' + '\n';
        }

        text += leftAlign('*Totals', c0Width) + ' : ' +
            rightAlign(tsRound(totalDuration) + 'ms', c1Width) + '   ' +
            rightAlign(String(selection.length), c2Width) + ' occurrences' +
            '\n';

        text += '\n';

        text += leftAlign('Selection start', c0Width) + ' : ' +
            rightAlign(tsRound(tsLo) + 'ms', c1Width) +
            '\n';
        text += leftAlign('Selection extent', c0Width) + ' : ' +
            rightAlign(tsRound(tsHi - tsLo) + 'ms', c1Width) +
            '\n';
      }

      // done
      var oldScrollTop = this.timelineContainer_.scrollTop;
      this.summaryEl_.textContent = text;
      this.timelineContainer_.scrollTop = oldScrollTop;
    }
  };

  return {
    TimelineView: TimelineView
  };
});
