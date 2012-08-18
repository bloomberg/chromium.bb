/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */


cr.define('performance_monitor', function() {
  'use strict';

  /**
   * Enum for time ranges, giving a descriptive name, time span prior to |now|,
   * data point resolution, and time-label frequency and format for each.
   * @enum {{
   *   value: number,
   *   name: string,
   *   timeSpan: number,
   *   resolution: number,
   *   labelEvery: number,
   *   format: string
   * }}
   * @private
   */
  var TimeRange_ = {
    // Prior 12 min, resolution of 1s, at most 720 points.
    // Labels at 60 point (1 min) intervals.
    minutes: {value: 0, name: 'Last 12 min', timeSpan: 720 * 1000,
        resolution: 1000, labelEvery: 60, format: 'MM-dd'},

    // Prior hour, resolution of 5s, at most 720 points.
    // Labels at 60 point (5 min) intervals.
    hour: {value: 1, name: 'Last Hour', timeSpan: 3600 * 1000,
        resolution: 1000 * 5, labelEvery: 60, format: 'MM-dd'},

    // Prior day, resolution of 2 min, at most 720 points.
    // Labels at 90 point (3 hour) intervals.
    day: {value: 2, name: 'Last Day', timeSpan: 24 * 3600 * 1000,
        resolution: 1000 * 60 * 2, labelEvery: 90, format: 'MM-dd'},

    // Prior week, resolution of 15 min -- at most 672 data points.
    // Labels at 96 point (daily) intervals.
    week: {value: 3, name: 'Last Week', timeSpan: 7 * 24 * 3600 * 1000,
        resolution: 1000 * 60 * 15, labelEvery: 96, format: 'M/d'},

    // Prior month (30 days), resolution of 1 hr -- at most 720 data points.
    // Labels at 168 point (weekly) intervals.
    month: {value: 4, name: 'Last Month', timeSpan: 30 * 24 * 3600 * 1000,
        resolution: 1000 * 3600, labelEvery: 168, format: 'M/d'},

    // Prior quarter (90 days), resolution of 3 hr -- at most 720 data points.
    // Labels at 112 point (fortnightly) intervals.
    quarter: {value: 5, name: 'Last Quarter', timeSpan: 90 * 24 * 3600 * 1000,
        resolution: 1000 * 3600 * 3, labelEvery: 112, format: 'M/yy'},
  };

  /*
   * Offset, in ms, by which to subtract to convert GMT to local time
   * @type {number}
   */
  var timezoneOffset = new Date().getTimezoneOffset() * 60000;

  /** @constructor */
  function PerformanceMonitor() {
    this.__proto__ = PerformanceMonitor.prototype;
    /**
     * All metrics have entries, but those not displayed have an empty div list.
     * If a div list is not empty, the associated data will be non-null, or
     * null but about to be filled by webui handler response. Thus, any metric
     * with non-empty div list but null data is awaiting a data response
     * from the webui handler. The webui handler uses numbers to uniquely
     * identify metric and event types, so we use the same numbers (in
     * string form) for the map key, repeating the id number in the |id|
     * field, as a number.
     * @type {Object.<string, {
     *                   id: number,
     *                   description: string,
     *                   units: string,
     *                   yAxis: !{max: number, color: string},
     *                   divs: !Array.<HTMLDivElement>,
     *                   data: ?Array.<{time: number, value: number}>
     *                 }>}
     * @private
     */
    this.metricMap_ = {};

    /*
     * Similar data for events, though no yAxis info is needed since events
     * are simply labelled markers at X locations. Rules regarding null data
     * with non-empty div list apply here as for metricMap_ above.
     * @type {Object.<number, {
     *   id: number,
     *   description: string,
     *   color: string,
     *   divs: !Array.<HTMLDivElement>,
     *   data: ?Array.<{time: number, longDescription: string}>
     * }>}
     * @private
     */
    this.eventMap_ = {};

    /**
     * Time periods in which the browser was active and collecting metrics
     * and events.
     * @type {!Array.<{start: number, end: number}>}
     * @private
     */
    this.intervals_ = [];

    this.setupTimeRangeChooser_();
    chrome.send('getAllEventTypes');
    chrome.send('getAllMetricTypes');
    this.setupMainChart_();
    TimeRange_.day.element.click();
  }

  PerformanceMonitor.prototype = {
    /**
     * Receive a list of all metrics, and populate |this.metricMap_| to
     * reflect said list. Reconfigure the checkbox set for metric selection.
     * @param {Array.<{metricType: string, shortDescription: string}>}
     *     allMetrics All metrics from which to select.
     */
    getAllMetricTypesCallback: function(allMetrics) {
      for (var i = 0; i < allMetrics.length; i++) {
        var metric = allMetrics[i];

        this.metricMap_[metric.metricType] = {
          id: metric.metricType,
          description: metric.shortDescription,
          units: metric.units,
          yAxis: {min: 0, max: metric.maxValue,
              color: jQuery.color.parse('blue')},
          divs: [],
          data: null
        };
      }

      this.setupCheckboxes_($('#choose-metrics')[0],
          this.metricMap_, this.addMetric, this.dropMetric);
    },

    /**
     * Receive a list of all event types, and populate |this.eventMap_| to
     * reflect said list. Reconfigure the checkbox set for event selection.
     * @param {Array.<{eventType: string, shortDescription: string}>}
     *   allEvents All events from which to select.
     */
    getAllEventTypesCallback: function(allEvents) {
      for (var i = 0; i < allEvents.length; i++) {
        var eventInfo = allEvents[i];

        this.eventMap_[eventInfo.eventType] = {
          id: eventInfo.eventType,
          color: jQuery.color.parse('red'),
          data: null,
          description: eventInfo.shortDescription,
          divs: []
        };
      }

      this.setupCheckboxes_($('#choose-events')[0],
          this.eventMap_, this.addEventType, this.dropEventType);
    },

    /**
     * Set up the radio button set to choose time range. Use div#radio-template
     * as a template.
     * @private
     */
    setupTimeRangeChooser_: function() {
      var timeDiv = $('#choose-time-range')[0];
      var radioTemplate = $('#radio-template')[0];

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
     * or metrics. Take a div |div| into which to place the checkboxes,
     * and a map |optionMap| with values that each include a property
     * |description|. Set up one checkbox for each entry in |optionMap|
     * labelled with that description. Arrange callbacks to function |check|
     * or |uncheck|, passing them the key of the checked or unchecked option,
     * when the relevant checkbox state changes.
     * @param {!HTMLDivElement} div A <div> into which to put checkboxes.
     * @param {!Object} optionMap A map of metric/event entries.
     * @param {!function(this:Controller, Object)} check
     *     The function to select an entry (metric or event).
     * @param {!function(this:Controller, Object)} uncheck
     *     The function to deselect an entry (metric or event).
     * @private
     */
    setupCheckboxes_: function(div, optionMap, check, uncheck) {
      var checkboxTemplate = $('#checkbox-template')[0];

      for (var option in optionMap) {
        var checkbox = checkboxTemplate.cloneNode(true);
        checkbox.querySelector('span').innerText = 'Show ' +
            optionMap[option].description;
        div.appendChild(checkbox);

        var input = checkbox.querySelector('input');
        input.option = optionMap[option].id;
        input.addEventListener('change', function(e) {
          (e.target.checked ? check : uncheck).call(this, e.target.option);
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
     * @param {!{start: number, end: number, resolution: number}} range
     *     The time range for which to get display data.
     */
    setTimeRange: function(range) {
      this.range = range;
      this.end = Math.floor(Date.now() / range.resolution) *
          range.resolution;

      this.start = this.end - range.timeSpan;
      this.requestIntervals();
    },

    /**
     * Request activity intervals in the current time range.
     */
    requestIntervals: function() {
      chrome.send('getActiveIntervals', [this.start, this.end]);
    },

    /**
     * Webui callback delivering response from requestIntervals call. Assumes
     * this is a new time range choice, which results in complete refresh of
     * all metrics and event types that are currently selected.
     * @param {!Array.<{start: number, end: number}>} intervals
     *     The new intervals.
     */
    getActiveIntervalsCallback: function(intervals) {
      this.intervals_ = intervals;

      for (var metric in this.metricMap_) {
        var metricValue = this.metricMap_[metric];
        if (metricValue.divs.length > 0)  // if we're displaying this metric.
          this.refreshMetric(metric);
      }

      for (var eventType in this.eventMap_) {
        var eventValue = this.eventMap_[eventType];
        if (eventValue.divs.length > 0)
          this.refreshEventType(eventValue.id);
      }
    },

    /**
     * Add a new metric to the main (currently only) chart.
     * @param {string} metric The metric to start displaying.
     */
    addMetric: function(metric) {
      this.metricMap_[metric].divs.push(this.charts[0]);
      this.refreshMetric(metric);
    },

    /**
     * Remove a metric from the chart(s).
     * @param {string} metric The metric to stop displaying.
     */
    dropMetric: function(metric) {
      var metricValue = this.metricMap_[metric];
      var affectedCharts = metricValue.divs;
      metricValue.divs = [];

      affectedCharts.forEach(this.drawChart, this);
    },

    /**
     * Request new metric data, assuming the metric table and chart already
     * exist.
     * @param {string} metric The metric for which to get data.
     */
    refreshMetric: function(metric) {
      var metricValue = this.metricMap_[metric];

      metricValue.data = null;  // Mark metric as awaiting response.
      chrome.send('getMetric', [metricValue.id,
          this.start, this.end, this.range.resolution]);
    },

    /**
     * Receive new datapoints for a metric, convert the data to Flot-usable
     * form, and redraw all affected charts.
     * @param {!{
     *   type: number,
     *   points: !Array.<{time: number, value: number}>
     * }} result An object giving metric ID, and time/value point pairs for
     *     that id.
     */
    getMetricCallback: function(result) {
      var metricValue = this.metricMap_[result.metricType];
      // Might have been dropped while waiting for data.
      if (metricValue.divs.length == 0)
        return;

      var series = [];
      metricValue.data = [series];  // Data ends with current open series.

      // Traverse the points, and the intervals, in parallel. Both are in
      // ascending time order. Create a sequence of data "series" (per Flot)
      // arrays, with each series comprising all points within a given interval.
      var interval = this.intervals_[0];
      var intervalIndex = 0;
      var pointIndex = 0;
      while (pointIndex < result.points.length &&
          intervalIndex < this.intervals_.length) {
        var point = result.points[pointIndex++];
        while (intervalIndex < this.intervals_.length &&
            point.time > interval.end) {
          interval = this.intervals_[++intervalIndex];  // Jump to new interval.
          if (series.length > 0) {
            series = [];  // Start a new series.
            metricValue.data.push(series);  // Put it on the end of the data.
          }
        }
        if (intervalIndex < this.intervals_.length &&
            point.time > interval.start) {
          series.push([point.time - timezoneOffset, point.value]);
        }
      }
      metricValue.divs.forEach(this.drawChart, this);
    },

    /**
     * Add a new event to the chart(s).
     * @param {string} eventType The type of event to start displaying.
     */
    addEventType: function(eventType) {
      // Events show on all charts.
      this.eventMap_[eventType].divs = this.charts;
      this.refreshEventType(eventType);
    },

    /*
     * Remove an event from the chart(s).
     * @param {string} eventType The type of event to stop displaying.
     */
    dropEventType: function(eventType) {
      var eventValue = this.eventMap_[eventType];
      var affectedCharts = eventValue.divs;
      eventValue.divs = [];

      affectedCharts.forEach(this.drawChart, this);
    },

    /**
     * Request new data for |eventType|, for times in the current range.
     * @param {string} eventType The type of event to get new data for.
     */
    refreshEventType: function(eventType) {
      // Mark eventType as awaiting response.
      this.eventMap_[eventType].data = null;

      chrome.send('getEvents', [eventType, this.start, this.end]);
    },

    /**
     * Receive new events for |eventType|. If |eventType| has been deselected
     * while awaiting webui handler response, do nothing. Otherwise, save the
     * data directly, since events are handled differently than metrics
     * when drawing (no "series"), and redraw all the affected charts.
     * @param {!{
     *   type: number,
     *   points: !Array.<{time: number, longDescription: string}>
     * }} result An object giving eventType ID, and time/description pairs for
     *     each event of that type in the range requested.
     */
    getEventsCallback: function(result) {
      var eventValue = this.eventMap_[result.eventType];

      if (eventValue.divs.length == 0)
        return;

      result.points.forEach(function(element) {
        element.time -= timezoneOffset;
      });

      eventValue.data = result.points;
      eventValue.divs.forEach(this.drawChart, this);
    },

    /**
     * Return an object containing an array of metrics and another of events
     * that include |chart| as one of the divs into which they display.
     * @param {HTMLDivElement} chart The <div> for which to get relevant items.
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
     * awaiting an asynchronous webui handler response to get their current
     * data).
     * @param {!{metrics: !Array,<Object>, events: !Array.<Object>}} chartData
     *     The event/metric data to check for readiness.
     * @return {boolean}  Whether data is ready.
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
     *                   description: string,
     *                   color: string,
     *                   data: !Array.<{time: number}>
     *                 }>} eventValues The events to make markings for.
     * @return {!Array.<{
     *                   color: string,
     *                   description: string,
     *                   xaxis: {from: number, to: number}
     *                 }>} A marks data structure for Flot to use.
     * @private
     */
    getEventMarks_: function(eventValues) {
      var markings = [];

      for (var i = 0; i < eventValues.length; i++) {
        var eventValue = eventValues[i];
        for (var d = 0; d < eventValue.data.length; d++) {
          var point = eventValue.data[d];
          if (point.time >= this.start - timezoneOffset &&
              point.time <= this.end - timezoneOffset) {
            markings.push({
              color: eventValue.color,
              description: eventValue.description,
              xaxis: {from: point.time, to: point.time}
            });
          } else {
            console.log('Event out of time range ' + this.start + ' -> ' +
                this.end + ' at: ' + point.time);
          }
        }
      }

      return markings;
    },

    /**
     * Redraw the chart in div |chart|, *if* all its dependent data is present.
     * Otherwise simply return, and await another call when all data is
     * available.
     * @param {HTMLDivElement} chart The <div> to redraw.
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

      // Ensure at least one y axis, as a reference for event placement.
      if (yAxes.length == 0)
        yAxes.push({max: 1.0});

      var markings = this.getEventMarks_(chartData.events);
      var chart = this.charts[0];
      var plot = $.plot(chart, seriesSeq, {
        yaxes: yAxes,
        xaxis: {
          mode: 'time',
          min: this.start - timezoneOffset,
          max: this.end - timezoneOffset
        },
        points: {show: true, radius: 1},
        lines: {show: true},
        grid: {markings: markings}
      });

      // For each event in |markings|, create also a label div, with left
      // edge colinear with the event vertical line. Top of label is
      // presently a hack-in, putting labels in three tiers of 25px height
      // each to avoid overlap. Will need something better.
      var labelTemplate = $('#label-template')[0];
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
});

var PerformanceMonitor = new performance_monitor.PerformanceMonitor();
