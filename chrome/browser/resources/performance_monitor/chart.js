/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

'use strict';

/* convert to cr.define('PerformanceMonitor', function() {...
 * when integrating into webui */

var Installer = function() {
  /**
   * Enum for time ranges, giving a descriptive name, time span prior to |now|,
   * data point resolution, and time-label frequency and format for each.
   * @enum {{
   *   value: !number,
   *   name: !string,
   *   timeSpan: !number,
   *   resolution: !number,
   *   labelEvery: !number,
   *   format: !string
   * }}
   * @private
   */
  var TimeRange_ = {
    // Prior day, resolution of 2 min, at most 720 points.
    // Labels at 90 point (3 hour) intervals.
    day: {value: 0, name: 'Last Day', timeSpan: 24 * 3600 * 1000,
        resolution: 1000 * 60 * 2, labelEvery: 90, format: 'MM-dd'},

    // Prior week, resolution of 15 min -- at most 672 data points.
    // Labels at 96 point (daily) intervals.
    week: {value: 1, name: 'Last Week', timeSpan: 7 * 24 * 3600 * 1000,
        resolution: 1000 * 60 * 15, labelEvery: 96, format: 'M/d'},

    // Prior month (30 days), resolution of 1 hr -- at most 720 data points.
    // Labels at 168 point (weekly) intervals.
    month: {value: 2, name: 'Last Month', timeSpan: 30 * 24 * 3600 * 1000,
        resolution: 1000 * 3600, labelEvery: 168, format: 'M/d'},

    // Prior quarter (90 days), resolution of 3 hr -- at most 720 data points.
    // Labels at 112 point (fortnightly) intervals.
    quarter: {value: 3, name: 'Last Quarter', timeSpan: 90 * 24 * 3600 * 1000,
        resolution: 1000 * 3600 * 3, labelEvery: 112, format: 'M/yy'},
  };

  /** @constructor */
  function PerformanceMonitor() {
    this.__proto__ = PerformanceMonitor.prototype;
    /**
     * All metrics have entries, but those not displayed have an empty div list.
     * If a div list is not empty, the associated data will be non-null, or
     * null but about to be filled by webui response. Thus, any metric with
     * non-empty div list but null data is awaiting a data response from the
     * webui.
     * @type {Object.<string, {
     *                   divs: !Array.<HTMLDivElement>,
     *                   yAxis: !{max: !number, color: !string},
     *                   data: ?Array.<{time: !number, value: !number}>,
     *                   description: !string,
     *                   units: !string
     *                 }>}
     * @private
     */
    this.metricMap_ = {
      jankiness: {
        divs: [],
        yAxis: {max: 100, color: 'rgb(255, 128, 128)'},
        data: null,
        description: 'Jankiness',
        units: 'milliJanks'
      },
      oddness: {
        divs: [],
        yAxis: {max: 20, color: 'rgb(0, 192, 0)'},
        data: null,
        description: 'Oddness',
        units: 'kOdds'
      }
    };

    /*
     * Similar data for events, though no yAxis info is needed since events
     * are simply labelled markers at X locations. Rules regarding null data
     * with non-empty div list apply here as for metricMap_ above.
     * @type {Object.<string, {
     *   divs: !Array.<HTMLDivElement>,
     *   description: !string,
     *   color: !string,
     *   data: ?Array.<{time: !number, longDescription: !string}>
     * }>}
     * @private
     */
    this.eventMap_ = {
      wampusAttacks: {
        divs: [],
        description: 'Wampus Attack',
        color: 'rgb(0, 0, 255)',
        data: null
      },
      solarEclipses: {
        divs: [],
        description: 'Solar Eclipse',
        color: 'rgb(255, 0, 255)',
        data: null
      }
    };

    /**
     * Time periods in which the browser was active and collecting metrics.
     * and events.
     * @type {!Array.<{start: !number, end: !number}>}
     * @private
     */
    this.intervals_ = [];

    this.setupCheckboxes_(
        '#chooseMetrics', this.metricMap_, this.addMetric, this.dropMetric);
    this.setupCheckboxes_(
        '#chooseEvents', this.eventMap_, this.addEventType, this.dropEventType);
    this.setupTimeRangeChooser_();
    this.setupMainChart_();
    TimeRange_.day.element.click().call(this);
  }

  PerformanceMonitor.prototype = {
    /**
     * Set up the radio button set to choose time range. Use div#radioTemplate
     * as a template.
     * @private
     */
    setupTimeRangeChooser_: function() {
      var timeDiv = $('#chooseTimeRange')[0];
      var radioTemplate = $('#radioTemplate')[0];

      for (var time in TimeRange_) {
        var timeRange = TimeRange_[time];
        var radio = radioTemplate.cloneNode(true);
        var input = radio.querySelector('input');

        input.value = timeRange.value;
        input.timeRange = timeRange;
        radio.querySelector('span').innerText = timeRange.name;
        timeDiv.appendChild(radio);
        timeRange.element = input;
      }

      timeDiv.addEventListener('click', function(e) {
        if (!e.target.webkitMatchesSelector('input[type="radio"]'))
          return;

        this.setTimeRange(e.target.timeRange);
      }.bind(this));
    },

    /**
     * Generalized function for setting up checkbox blocks for either events
     * or metrics. Take a div ID |divId| into which to place the checkboxes,
     * and a map |optionMap| with values that each include a property
     * |description|. Set up one checkbox for each entry in |optionMap|
     * labelled with that description. Arrange callbacks to function |check|
     * or |uncheck|, passing them the key of the checked or unchecked option,
     * when the relevant checkbox state changes.
     * @param {!string} divId Id of division into which to put checkboxes
     * @param {!Object} optionMap map of metric/event entries
     * @param {!function(this:Controller, Object)} check
     *     function to select an entry (metric or event)
     * @param {!function(this:Controller, Object)} uncheck
     *     function to deselect an entry (metric or event)
     * @private
     */
    setupCheckboxes_: function(divId, optionMap, check, uncheck) {
      var checkboxTemplate = $('#checkboxTemplate')[0];
      var chooseMetricsDiv = $(divId)[0];

      for (var option in optionMap) {
        var checkbox = checkboxTemplate.cloneNode(true);
        checkbox.querySelector('span').innerText = 'Show ' +
            optionMap[option].description;
        chooseMetricsDiv.appendChild(checkbox);

        var input = checkbox.querySelector('input');
        input.option = option;
        input.addEventListener('change', function(e) {
          if (e.target.checked)
            check.call(this, e.target.option);
          else
            uncheck.call(this, e.target.option);
        }.bind(this));
      }
    },

    /**
     * Set up just one chart in which all metrics will be displayed
     * initially. But, the design readily accommodates addition of
     * new charts, and movement of metrics into those other charts.
     * @private
     */
    setupMainChart_: function() {
      this.chartParent = $('#charts')[0];
      this.charts = [document.createElement('div')];
      this.charts[0].className = 'chart';
      this.chartParent.appendChild(this.charts[0]);
    },

    /**
     * Set the time range for which to display metrics and events. For
     * now, the time range always ends at "now", but future implementations
     * may allow time ranges not so anchored.
     * @param {!{start: !number, end: !number, resolution: !number}} range
     */
    setTimeRange: function(range) {
      this.range = range;
      this.end = Math.floor(Date.now() / range.resolution) *
          range.resolution;

      // Take the GMT value of this.end ("now") and subtract from it the
      // number of minutes by which we lag GMT in the present timezone,
      // X mS/minute. This will show time in the present timezone.
      this.end -= new Date().getTimezoneOffset() * 60000;
      this.start = this.end - range.timeSpan;
      this.requestIntervals();
    },

    /**
     * Return mock interval set for testing.
     * @return {!Array.<{start: !number, end: !number}>} intervals
     */
    getMockIntervals: function() {
      var interval = this.end - this.start;

      return [
        {start: this.start + interval * .1,
         end: this.start + interval * .2},
        {start: this.start + interval * .7,
         end: this.start + interval}
      ];
    },

    /**
     * Request activity intervals in the current time range.
     */
    requestIntervals: function() {
      this.onReceiveIntervals(this.getMockIntervals());
      // Replace with: chrome.send('getIntervals', this.start, this.end,
      // this.onReceiveIntervals);
    },

    /**
     * Webui callback delivering response from requestIntervals call. Assumes
     * this is a new time range choice, which results in complete refresh of
     * all metrics and event types that are currently selected.
     * @param {!Array.<{start: !number, end: !number}>} intervals new intervals
     */
    onReceiveIntervals: function(intervals) {
      this.intervals_ = intervals;

      for (var metric in this.metricMap_) {
        var metricValue = this.metricMap_[metric];
        if (metricValue.divs.length > 0)  // if we're displaying this metric.
          this.refreshMetric(metric);
      }

      for (var eventType in this.eventMap_) {
        var eventValue = this.eventMap_[eventType];
        if (eventValue.divs.length > 0)
          this.refreshEventType(eventType);
      }
    },

    /**
     * Add a new metric to the main (currently only) chart.
     * @param {!string} metric Metric to start displaying
     */
    addMetric: function(metric) {
      this.metricMap_[metric].divs.push(this.charts[0]);
      this.refreshMetric(metric);
    },

    /**
     * Remove a metric from the chart(s).
     * @param {!string} metric Metric to stop displaying
     */
    dropMetric: function(metric) {
      var metricValue = this.metricMap_[metric];
      var affectedCharts = metricValue.divs;
      metricValue.divs = [];

      affectedCharts.forEach(this.drawChart, this);
    },

    /**
     * Return mock metric data points for testing. Give values ranging from
     * offset to max-offset. (This lets us avoid direct overlap of
     * different mock data sets in an ugly way that will die in the next
     * version anyway.)
     * @param {!number} max Max data value to return, less offset
     * @param {!number} offset Adjustment factor
     * @return {!Array.<{time: !number, value: !number}>}
     */
    getMockDataPoints: function(max, offset) {
      var dataPoints = [];

      for (var i = 0; i < this.intervals_.length; i++) {
        // Rise from low offset to high max-offset in 100 point steps.
        for (var time = this.intervals_[i].start;
             time <= this.intervals_[i].end; time += this.range.resolution) {
          var numPoints = time / this.range.resolution;
          dataPoints.push({time: time, value: offset + (numPoints % 100) *
              (max - 2 * offset) / 100});
        }
      }
      return dataPoints;
    },

    /**
     * Request new metric data, assuming the metric table and chart already
     * exist.
     * @param {!string} metric Metric for which to get data
     */
    refreshMetric: function(metric) {
      var metricValue = this.metricMap_[metric];

      metricValue.data = null;  // Mark metric as awaiting response.
      this.onReceiveMetric(metric,
          this.getMockDataPoints(metricValue.yAxis.max, 5));
      // Replace with:
      // chrome.send("getMetric", this.range.start, this.range.end,
      //  this.range.resolution, this.onReceiveMetric);
    },

    /**
     * Receive new datapoints for |metric|, convert the data to Flot-usable
     * form, and redraw all affected charts.
     * @param {!string} metric Metric to which |points| applies
     * @param {!Array.<{time: !number, value: !number}>} points
     *     new data points
     */
    onReceiveMetric: function(metric, points) {
      var metricValue = this.metricMap_[metric];

      // Might have been dropped while waiting for data.
      if (metricValue.divs.length == 0)
        return;

      var series = [];
      metricValue.data = [series];

      // Traverse the points, and the intervals, in parallel. Both are in
      // ascending time order. Create a sequence of data "series" (per Flot)
      // arrays, with each series comprising all points within a given interval.
      var interval = this.intervals_[0];
      var intervalIndex = 0;
      var pointIndex = 0;
      while (pointIndex < points.length &&
          intervalIndex < this.intervals_.length) {
        var point = points[pointIndex++];
        while (intervalIndex < this.intervals_.length &&
            point.time > interval.end) {
          interval = this.intervals_[++intervalIndex]; // Jump to new interval.
          if (series.length > 0) {
            series = [];                   // Start a new series.
            metricValue.data.push(series); // Put it on the end of the data.
          }
        }
        if (intervalIndex < this.intervals_.length &&
            point.time > interval.start)
          series.push([point.time, point.value]);
      }

      metricValue.divs.forEach(this.drawChart, this);
    },

    /**
     * Add a new event to the chart(s).
     * @param {!string} eventType type of event to start displaying
     */
    addEventType: function(eventType) {
      // Events show on all charts.
      this.eventMap_[eventType].divs = this.charts;
      this.refreshEventType(eventType);
    },

    /*
     * Remove an event from the chart(s).
     * @param {!string} eventType type of event to stop displaying
     */
    dropEventType: function(eventType) {
      var eventValue = this.eventMap_[eventType];
      var affectedCharts = eventValue.divs;
      eventValue.divs = [];

      affectedCharts.forEach(this.drawChart, this);
    },

    /**
     * Return mock event points for testing.
     * @param {!string} eventType type of event to generate mock data for
     * @return {!Array.<{time: !number, longDescription: !string}>}
     */
    getMockEventValues: function(eventType) {
      var mockValues = [];
      for (var i = 0; i < this.intervals_.length; i++) {
        var interval = this.intervals_[i];

        mockValues.push({
            time: interval.start,
            longDescription: eventType + ' at ' +
            new Date(interval.start) + ' blah, blah blah'});
        mockValues.push({
            time: (interval.start + interval.end) / 2,
            longDescription: eventType + ' at ' +
            new Date((interval.start + interval.end) / 2) + ' blah, blah'});
        mockValues.push({
            time: interval.end,
            longDescription: eventType + ' at ' + new Date(interval.end) +
            ' blah, blah blah'});
      }
      return mockValues;
    },

    /**
     * Request new data for |eventType|, for times in the current range.
     * @param {!string} eventType type of event to get new data for
     */
    refreshEventType: function(eventType) {
      // Mark eventType as awaiting response.
      this.eventMap_[eventType].data = null;
      this.onReceiveEventType(eventType, this.getMockEventValues(eventType));
      // Replace with:
      // chrome.send("getEvents", eventType, this.range.start, this.range.end);
    },

    /**
     * Receive new data for |eventType|. If the event has been deselected while
     * awaiting webui response, do nothing. Otherwise, save the data directly,
     * since events are handled differently than metrics when drawing
     * (no "series"), and redraw all the affected charts.
     * @param {!string} eventType type of event the new data applies to
     * @param {!Array.<{time: !number, longDescription: !string}>} values
     *     new event values
     */
    onReceiveEventType: function(eventType, values) {
      var eventValue = this.eventMap_[eventType];

      if (eventValue.divs.length == 0)
        return;

      eventValue.data = values;
      eventValue.divs.forEach(this.drawChart, this);
    },

    /**
     * Return an object containing an array of metrics and another of events
     * that include |chart| as one of the divs into which they display.
     * @param {HTMLDivElement} chart div for which to get relevant items
     * @return {!{metrics: !Array,<Object>, events: !Array.<Object>}}
     * @private
     */
    getChartData_: function(chart) {
      var result = {metrics: [], events: []};

      for (var metric in this.metricMap_) {
        var metricValue = this.metricMap_[metric];

        if (metricValue.divs.indexOf(chart) != -1)
          result.metrics.push(metricValue);
      }

      for (var eventType in this.eventMap_) {
        var eventValue = this.eventMap_[eventType];

        // Events post to all divs, if they post to any.
        if (eventValue.divs.length > 0)
          result.events.push(eventValue);
      }

      return result;
    },

    /**
     * Check all entries in an object of the type returned from getChartData,
     * above, to see if all events and metrics have completed data (none is
     * awaiting an asynchronous webui response to get their current data).
     * @param {!{metrics: !Array,<Object>, events: !Array.<Object>}} chartData
     *     event/metric data to check for readiness
     * @return {boolean} is data ready?
     * @private
     */
    isDataReady_: function(chartData) {
      for (var i = 0; i < chartData.metrics.length; i++) {
        if (!chartData.metrics[i].data)
          return false;
      }

      for (var i = 0; i < chartData.events.length; i++) {
        if (!chartData.events[i].data)
          return false;
      }

      return true;
    },

    /**
     * Create and return an array of "markings" (per Flot), representing
     * vertical lines at the event time, in the event's color. Also add
     * (not per Flot) a |description| property to each, to be used for hand
     * creating description boxes.
     * @param {!Array.<{
     *                   description: !string,
     *                   color: !string,
     *                   data: !Array.<{time: !number}>
     *                 }>} eventValues events to make markings for
     * @return {!Array.<{
     *                   color: !string,
     *                   description: !string,
     *                   xaxis: {from: !number, to: !number}
     *                 }>} mark data structure for Flot to use
     * @private
     */
    getEventMarks_: function(eventValues) {
      var markings = [];

      for (var i = 0; i < eventValues.length; i++) {
        var eventValue = eventValues[i];
        for (var d = 0; d < eventValue.data.length; d++) {
          var point = eventValue.data[d];
          markings.push({
            color: eventValue.color,
            description: eventValue.description,
            xaxis: {from: point.time, to: point.time}
          });
        }
      }

      return markings;
    },

    /**
     * Redraw the chart in div |chart|, *if* all its dependent data is present.
     * Otherwise simply return, and await another call when all data is
     * available.
     * @param {HTMLDivElement} chart div to redraw
     */
    drawChart: function(chart) {
      var chartData = this.getChartData_(chart);

      if (!this.isDataReady_(chartData))
        return;

      var seriesSeq = [];
      var yAxes = [];
      chartData.metrics.forEach(function(value) {
        yAxes.push(value.yAxis);
        for (var i = 0; i < value.data.length; i++) {
          seriesSeq.push({
            color: value.yAxis.color,
            data: value.data[i],
            label: i == 0 ? value.description + ' (' + value.units + ')' : null,
            yaxis: yAxes.length,  // Use just-added Y axis.
          });
        }
      });

      var markings = this.getEventMarks_(chartData.events);
      var chart = this.charts[0];
      var plot = $.plot(chart, seriesSeq, {
        yaxes: yAxes,
        xaxis: {mode: 'time'},
        grid: {markings: markings}
      });

      // For each event in |markings|, create also a label div, with left
      // edge colinear with the event vertical-line. Top of label is
      // presently a hack-in, putting labels in three tiers of 25px height
      // each to avoid overlap. Will need something better.
      var labelTemplate = $('#labelTemplate')[0];
      for (var i = 0; i < markings.length; i++) {
        var mark = markings[i];
        var point =
            plot.pointOffset({x: mark.xaxis.to, y: yAxes[0].max, yaxis: 1});
        var labelDiv = labelTemplate.cloneNode(true);
        labelDiv.innerText = mark.description;
        labelDiv.style.left = point.left + 'px';
        labelDiv.style.top = (point.top + 25 * (i % 3)) + 'px';
        chart.appendChild(labelDiv);
      }
    }
  };
  return {
    PerformanceMonitor: PerformanceMonitor
  };
}();

var performanceMonitor = new Installer.PerformanceMonitor();
