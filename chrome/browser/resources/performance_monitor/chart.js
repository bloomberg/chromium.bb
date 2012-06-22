/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

'use strict';

google.load('visualization', '1', {packages: ['corechart']});

function $(criterion) {
  return document.querySelector(criterion);
}

var controller = new function() {
  // Tabular setup for various time ranges, giving a descriptive name, time span
  // prior to |now|, data point resolution, and time-label frequency and format
  // for each.
  this.TimeRange = {
    // Prior day, resolution of 2 min, at most 720 points,
    // Labels at 90 point (3 hour) intervals
    day: {value: 0, name: 'Last Day', timeSpan: 24 * 3600 * 1000,
        resolution: 1000 * 60 * 2, labelEvery: 90, format: 'MM-dd'},

    // Prior week, resolution of 15 min -- at most 672 data points
    // Labels at 96 point (daily) intervals
    week: {value: 1, name: 'Last Week', timeSpan: 7 * 24 * 3600 * 1000,
        resolution: 1000 * 60 * 15, labelEvery: 96, format: 'M/d'},

    // Prior month (30 days), resolution of 1 hr -- at most 720 data points
    // Labels at 168 point (weekly) intervals
    month: {value: 2, name: 'Last Month', timeSpan: 30 * 24 * 3600 * 1000,
        resolution: 1000 * 3600, labelEvery: 168, format: 'M/d'},

    // Prior quarter (90 days), resolution of 3 hr -- at most 720 data points
    // Labels at 112 point (fortnightly) intervals
    quarter: {value: 3, name: 'Last Quarter', timeSpan: 90 * 24 * 3600 * 1000,
        resolution: 1000 * 3600 * 3, labelEvery: 112, format: 'M/yy'},
  };

  // Parent container of all line graphs
  this.chartDiv = $('#charts');

  // Parent container of checkboxes to choose metrics to display
  this.chooseMetricsDiv = $('#chooseMetrics');

  // Parent container of checkboxes to choose event types to display
  this.chooseEventsDiv = $('#chooseEvents');

  // Parent container of radio buttons to select time range
  this.timeDiv = $('#chooseTimeRange');

  this.metricMap = {}; // MetricName => {div, lineChart, dataTable} objects
  this.eventMap = {};  // EventName => event point lists, as returned by webui
  this.intervals = []; // Array of objects {start, end}

  this.setTimeRange = function(range) {
    this.range = range;
    this.end = Math.floor(new Date().getTime() / range.resolution) *
        range.resolution;
    this.start = this.end - range.timeSpan;
    this.requestIntervals();
  }

  // Return mock interval set for testing
  this.getMockIntervals = function() {
    var interval = this.end - this.start;
    return [
        {'start': this.start + interval * .1,
        'end': this.start + interval * .2},
        {'start': this.start + interval * .7, 'end': this.start + interval * .9}
    ];
  }
  // Request array of objects with start and end fields showing browser
  // activity intervals in the specified time range
  this.requestIntervals = function() {
    this.onReceiveIntervals(this.getMockIntervals());
    // Replace with: chrome.send('getIntervals', this.start, this.end,
    // this.onReceiveIntervals);
  }

  // Webui callback delivering response from requestIntervals call.  Assumes
  // this is a new time range choice, which results in complete refresh of
  // all metrics and event types that are currently selected.
  this.onReceiveIntervals = function(intervals) {
    this.intervals = intervals;
    for (var metric in this.metricMap)
      this.refreshMetric(metric);
    for (var eventType in this.eventMap)
      this.refreshEvent(eventType);
  }

  // Add a new metric, and its associated linegraph. The linegraph for
  // each metric has a discrete domain of times.  This is not continuous
  // because of breaks for each interval of activity.  (No point in showing
  // a lot of "dead air" when the browser wasn't running.)  Column 1 of
  // its DataTable is the metric data, and higher numbered columns are added
  // in pairs for each event type currently chosen.  Each pair gives the
  // event occurence points, and mouseover detailed description for one event
  // type.
  this.addMetric = function(metric) {
    if (!(metric in this.metricMap)) {
      var div = document.createElement('div');

      div.className = 'chart';
      this.chartDiv.appendChild(div);

      var table = new google.visualization.DataTable();
      var chart = new google.visualization.LineChart(div);
      this.metricMap[metric] = {'div': div, 'chart': chart, 'table': table};

      table.addColumn('string', 'Time');  // Default domain column
      table.addColumn('number', 'Value'); // Only numerical range column
      for (var event in this.events)
        this.addEventColumns(table, event);

      this.refreshMetric(metric);
    }
  }

  // Remove a metric from the UI
  this.dropMetric = function(metric) {
    if (metric in this.metricMap) {
      this.chartDiv.removeChild(this.metricMap[metric].div);
      delete this.metricMap[metric];
    }
  }

  // Return mock metric data points for testing
  this.getMockDataPoints = function() {
    var dataPoints = [];

    for (var x = 0; x < this.intervals.length; x++) {
      // Rise from low 0 to high 100 every 20 min
      for (var time = this.intervals[x].start; time <= this.intervals[x].end;
          time += this.range.resolution)
        dataPoints.push({'time': time, 'value': time % 1000000 / 10000});
    }
    return dataPoints;
  }

  // Request new metric data, assuming the metric table and chart already
  // exist.
  this.refreshMetric = function(metric) {
    this.onReceiveMetric(metric, this.getMockDataPoints());
    // Replace with:
    // chrome.send("getMetric", this.range.start, this.range.end,
    //  this.range.resolution, this.onReceiveMetric);
  }

  // Receive new datapoints for |metric|, and completely refresh the DataTable
  // for that metric, redrawing the chart.  (We cannot preserve the event
  // columns because entirely new rows may be implied by the new metric
  // datapoints.)
  this.onReceiveMetric = function(metric, points) {
    // Might have been dropped while waiting for data
    if (!(metric in this.metricMap))
      return;

    var data = this.metricMap[metric].table;

    data.removeRows(0, data.getNumberOfRows());

    // Traverse the points, which are in time order, and the intervals,
    // placing each value in the interval (if any) in which it belongs.
    var interval = this.intervals[0];
    var intervalIndex = 0;
    var valueIndex = 0;
    var value;
    while (valueIndex < points.length &&
        intervalIndex < this.intervals.length) {
      value = points[valueIndex++];
      while (value.time > interval.end &&
          intervalIndex < this.intervals.length) {
        interval = this.intervals[++intervalIndex]; // Jump to new interval
        data.addRow(null, null);                    // Force gap in line chart
      }
      if (intervalIndex < this.intervals.length && value.time > interval.start)
        if (data.getNumberOfRows() % this.range.labelEvery == 0)
          data.addRow([new Date(value.time).toString(this.range.format),
                      value.value]);
        else
          data.addRow(['', value.value]);
     }
     this.drawChart(metric);
  }

  // Add a new event to all line graphs.
  this.addEventType = function(eventType) {
    if (!(eventType in this.eventMap)) {
      this.eventMap[eventType] = [];
      this.refreshEventType(eventType);
    }
  }

  // Return mock event point for testing
  this.getMockEventValues = function(eventType) {
    var mockValues = [];
    for (var i = 0; i < this.intervals.length; i++) {
      var interval = this.intervals[i];

      mockValues.push({
          time: interval.start,
          shortDescription: eventType,
          longDescription: eventType + ' at ' +
          new Date(interval.start) + ' blah, blah blah'});
      mockValues.push({
          time: (interval.start + interval.end) / 2,
          shortDescription: eventType,
          longDescription: eventType + ' at ' +
          new Date((interval.start + interval.end) / 2) + ' blah, blah blah'});
      mockValues.push({
          time: interval.end,
          shortDescription: eventType,
          longDescription: eventType + ' at ' + new Date(interval.end) +
          ' blah, blah blah'});
    }
    return mockValues;
  }

  // Request new data for |eventType|, for times in the current range.
  this.refreshEventType = function(eventType) {
    this.onReceiveEventType(eventType, this.getMockEventValues(eventType));
    // Replace with:
    // chrome.send("getEvents", eventType, this.range.start, this.range.end);
  }

  // Add an event column pair to DataTable |table| for |eventType|
  this.addEventColumns = function(table, eventType) {
    var annotationCol = table.addColumn({'id': eventType, type: 'string',
        role: 'annotation'});
    var rolloverCol = table.addColumn({'id': eventType + 'Tip', type: 'string',
        role: 'annotationText'});
    var values = this.eventMap[eventType];
    var interval = this.intervals[0], intervalIndex = 0;

    for (var i = 0; i < values.length; i++) {
      var event = values[i];
      var rowIndex = 0;
      while (event.time > interval.end &&
          intervalIndex < this.intervals.length - 1)
      {
        // Skip interval times, inclusive of interval.end, and of following null
        rowIndex += (interval.end - interval.start) / this.range.resolution + 2;
        interval = this.intervals[++intervalIndex];
      }
      if (event.time >= interval.start && event.time <= interval.end) {
        table.setCell(rowIndex + (event.time - interval.start) /
            this.range.resolution, annotationCol, event.shortDescription);
        table.setCell(rowIndex + (event.time - interval.start) /
            this.range.resolution, rolloverCol, event.longDescription);
      }
    }
  }

  this.dropEventColumns = function(table, eventType) {
    var colIndex, numCols = table.getNumberOfColumns();

    for (colIndex = 0; colIndex < numCols; colIndex++)
      if (table.getColumnId(colIndex) == eventType)
        break;

    if (colIndex < numCols) {
       table.removeColumn(colIndex + 1);
       table.removeColumn(colIndex);
    }
  }

  // Receive new data for |eventType|.  Save this in eventMap for future
  // redraws.  Then, for each metric linegraph remove any current column pair
  // for |eventType| and replace it with a new pair, which will reflect the
  // new data.  Redraw the linegraph.
  this.onReceiveEventType = function(eventType, values) {
    this.eventMap[eventType] = values;

    for (var metric in this.metricMap) {
      var table = this.metricMap[metric].table;

      this.dropEventColumns(table, eventType);
      this.addEventColumns(table, eventType);
      this.drawChart(metric);
    }
  }

  this.dropEventType = function(eventType) {
    delete this.eventMap[eventType];

    for (var metric in this.metricMap) {
      var table = this.metricMap[metric].table;

      this.dropEventColumns(table, eventType);
      this.drawChart(metric);
    }
  }


  // Redraw the linegraph for |metric|, assuming its DataTable is fully up to
  // date.
  this.drawChart = function(metric) {
    var entry = this.metricMap[metric];

    entry.chart.draw(entry.table, {title: metric + ' for ' + this.range.name,
        hAxis: {showTextEvery: this.range.labelEvery}});
  }

  this.setupTimeRangeChooser = function() {
    var controller = this;
    var radioTemplate = $('#radioTemplate');

    for (var time in this.TimeRange) {
       var range = this.TimeRange[time];
       var radio = radioTemplate.cloneNode(true);
       var input = radio.querySelector('input');

       input.value = range.value;
       radio.querySelector('span').innerText = range.name;
       this.timeDiv.appendChild(radio);
       range.element = input;
       radio.range = range;
       radio.addEventListener('click', function() {
          controller.setTimeRange(this.range);
       });
    }
  }

  this.setupMetricChooser = function(metricTypes) {
    var checkboxTemplate = $('#checkboxTemplate');

    metricTypes.forEach(function(metric) {
      var checkbox = checkboxTemplate.cloneNode(true);
      var input = checkbox.querySelector('input');
      input.addEventListener('change', function() {
        if (input.checked)
          this.addMetric(metric);
        else
          this.dropMetric(metric);
        }.bind(this));
      checkbox.getElementsByTagName('span')[0].innerText = 'Show ' + metric;
      this.chooseMetricsDiv.appendChild(checkbox);
    }, this);
  }

  this.setupEventChooser = function(eventTypes) {
    var checkboxTemplate = $('#checkboxTemplate');

    eventTypes.forEach(function(event) {
      var checkbox = checkboxTemplate.cloneNode(true);
      var input = checkbox.querySelector('input');
      input.addEventListener('change', function() {
        if (input.checked)
          this.addEventType(event);
        else
          this.dropEventType(event);
        }.bind(this));
      checkbox.getElementsByTagName('span')[0].innerText = 'Show ' + event;
      this.chooseEventsDiv.appendChild(checkbox);
    }, this);
  }

  this.setupMetricChooser(['Oddness', 'Jankiness']);
  this.setupEventChooser(['Wampus Attack', 'Solar Eclipse']);
  this.setupTimeRangeChooser();
  this.TimeRange.day.element.click();
}
