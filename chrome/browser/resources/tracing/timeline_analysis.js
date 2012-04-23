// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview TimelineView visualizes TRACE_EVENT events using the
 * tracing.Timeline component and adds in selection summary and control buttons.
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


  function getTextForSelection(selection) {
    var text = '';
    var sliceHits = selection.getSliceHits();
    var counterSampleHits = selection.getCounterSampleHits();

    if (sliceHits.length == 1) {
      var c0Width = 14;
      var slice = sliceHits[0].slice;
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
    } else if (sliceHits.length > 1) {
      var c0Width = 55;
      var c1Width = 12;
      var c2Width = 5;
      text = 'Slices:\n';
      var tsLo = sliceHits.range.min;
      var tsHi = sliceHits.range.max;

      // compute total sliceHits duration
      var titles = sliceHits.map(function(i) { return i.slice.title; });

      var slicesByTitle = {};
      for (var i = 0; i < sliceHits.length; i++) {
        var slice = sliceHits[i].slice;
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
          rightAlign(String(sliceHits.length), c2Width) + ' occurrences' +
          '\n';

      text += '\n';

      text += leftAlign('Selection start', c0Width) + ' : ' +
          rightAlign(tsRound(tsLo) + 'ms', c1Width) +
          '\n';
      text += leftAlign('Selection extent', c0Width) + ' : ' +
          rightAlign(tsRound(tsHi - tsLo) + 'ms', c1Width) +
          '\n';
    }

    if (counterSampleHits.length == 1) {
      text = 'Selected counter:\n';
      var c0Width = 55;
      var hit = counterSampleHits[0];
      var ctr = hit.counter;
      var sampleIndex = hit.sampleIndex;
      var values = [];
      for (var i = 0; i < ctr.numSeries; ++i)
        values.push(ctr.samples[ctr.numSeries * sampleIndex + i]);
      text += leftAlign('Title', c0Width) + ': ' + ctr.name + '\n';
      text += leftAlign('Timestamp', c0Width) + ': ' +
        tsRound(ctr.timestamps[sampleIndex]) + ' ms\n';
      if (ctr.numSeries > 1)
        text += leftAlign('Values', c0Width) + ': ' + values.join('\n') + '\n';
      else
        text += leftAlign('Value', c0Width) + ': ' + values.join('\n') + '\n';

    } else if (counterSampleHits.length > 1 && sliceHits.length == 0) {
      text += 'Analysis of multiple counters not yet implemented. ' +
          'Pick a single counter.';
    }
    return text;
  }

  var TimelineAnalysisView = cr.ui.define('div');

  TimelineAnalysisView.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'timeline-analysis';
    },

    set selection(selection) {
      this.textContent = getTextForSelection(selection);
    }
  };

  return {
    TimelineAnalysisView: TimelineAnalysisView,
  };
});
