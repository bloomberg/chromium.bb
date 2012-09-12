/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('performance_monitor', function() {
  'use strict';

  /**
   * Enum for time ranges, giving for each a descriptive name, time span in ms
   * prior to |now|, data point resolution in ms, time-label frequency in data
   * points per label, and format using Date.js standards, for each.
   *
   * The additional |element| field is added in setupTimeRangeTab_, giving
   * the HTML input tag for the radio button selecting the given time range.
   * @enum {{
   *   value: number,
   *   name: string,
   *   timeSpan: number,
   *   resolution: number,
   *   labelEvery: number,
   *   format: string,
   *   element: HTMLElement
   * }}
   * @private
   */
  var TimeRange_ = {
    // Prior 15 min, resolution of 5s, at most 180 points.
    // Labels at 12 point (1 min) intervals.
    minutes: {value: 0, name: 'Last 15 min', timeSpan: 900 * 1000,
        resolution: 1000 * 5, labelEvery: 12, format: 'MM-dd'},

    // Prior hour, resolution of 20s, at most 180 points.
    // Labels at 15 point (5 min) intervals.
    hour: {value: 1, name: 'Last Hour', timeSpan: 3600 * 1000,
        resolution: 1000 * 20, labelEvery: 15, format: 'MM-dd'},

    // Prior day, resolution of 5 min, at most 288 points.
    // Labels at 36 point (3 hour) intervals.
    day: {value: 2, name: 'Last Day', timeSpan: 24 * 3600 * 1000,
        resolution: 1000 * 60 * 5, labelEvery: 36, format: 'MM-dd'},

    // Prior week, resolution of 1 hr -- at most 168 data points.
    // Labels at 24 point (daily) intervals.
    week: {value: 3, name: 'Last Week', timeSpan: 7 * 24 * 3600 * 1000,
        resolution: 1000 * 3600, labelEvery: 24, format: 'M/d'},

    // Prior month (30 days), resolution of 4 hr -- at most 180 data points.
    // Labels at 42 point (weekly) intervals.
    month: {value: 4, name: 'Last Month', timeSpan: 30 * 24 * 3600 * 1000,
        resolution: 1000 * 3600 * 4, labelEvery: 42, format: 'M/d'},

    // Prior quarter (90 days), resolution of 12 hr -- at most 180 data points.
    // Labels at 28 point (fortnightly) intervals.
    quarter: {value: 5, name: 'Last Quarter', timeSpan: 90 * 24 * 3600 * 1000,
        resolution: 1000 * 3600 * 12, labelEvery: 28, format: 'M/yy'},
  };

  /*
   * Table of colors to use for metrics and events. Basically boxing the
   * colorwheel, but leaving out yellows and fully saturated colors.
   * @type {Array.<string>}
   * @private
   */
  var ColorTable_ = [
    'rgb(255, 128, 128)', 'rgb(128, 255, 128)', 'rgb(128, 128, 255)',
    'rgb(128, 255, 255)', 'rgb(255, 128, 255)', // No bright yellow
    'rgb(255,  64,  64)', 'rgb( 64, 255,  64)', 'rgb( 64,  64, 255)',
    'rgb( 64, 255, 255)', 'rgb(255,  64, 255)', // No medium yellow either
    'rgb(128,  64,  64)', 'rgb( 64, 128,  64)', 'rgb( 64,  64, 128)',
    'rgb( 64, 128, 128)', 'rgb(128,  64, 128)', 'rgb(128, 128,  64)'
  ];

  /*
   * Offset, in ms, by which to subtract to convert GMT to local time.
   * @type {number}
   * @private
   */
  var timezoneOffset_ = new Date().getTimezoneOffset() * 60000;

  /*
   * Additional range multiplier to ensure that points don't hit the top of
   * the graph.
   * @type {number}
   * @private
   */
  var yAxisMargin_ = 1.05;

  /*
   * Number of time resolution periods to wait between automated update of
   * graphs.
   * @type {number}
   * @private
   */
  var intervalMultiple_ = 2;

  /*
   * Number of milliseconds to wait before deciding that the most recent
   * resize event is not going to be followed immediately by another, and
   * thus needs handling.
   * @type {number}
   * @private
   */
  var resizeDelay_ = 500;

  /** @constructor */
  function PerformanceMonitor() {
    this.__proto__ = PerformanceMonitor.prototype;

    /**
     * Detailed information on a metric in the UI. MetricId is a unique
     * identifying number for the metric, provided by the webui, and assumed
     * to be densely populated. All metrics also have a description and
     * an associated category giving their unit information and home chart.
     * They also have a color in which they are displayed, and a maximum value
     * by which to scale their y-axis.
     *
     * Although in the present UI each metric appears only in the home chart of
     * its metric category, we keep the divs property to allow future
     * modifications in which the same metric might appear in several charts
     * for side-by-side comparisons. Metrics not being displayed have an
     * empty div list. If a div list is not empty, the associated data will
     * be non-null, or null but about to be filled by webui handler response.
     * Thus, any metric with non-empty div list but null data is awaiting a
     * data response from the webui handler.
     * @typedef {{
     *   metricId: number,
     *   description: string,
     *   category: !Object,
     *   color: string,
     *   maxValue: number,
     *   divs: !Array.<HTMLDivElement>,
     *   data: ?Array.<{time: number, value: number}>
     * }}
     */
    PerformanceMonitor.MetricDetails;

    /**
     * Similar data for events as for metrics, though no y-axis info is needed
     * since events are simply labelled markers at X locations. Rules regarding
     * null data with non-empty div list apply here as for metricDetailsMap_
     * above. The |data| field follows a special rule not describable in
     * JSDoc:  Aside from the |time| key, each event type has varying other
     * properties, with unknown key names, which properties must still be
     * displayed. Such properties always have value of form
     * {label: 'some label', value: 'some value'}, with label and value
     * internationalized.
     * @typedef {{
     *   eventId: number,
     *   name: string,
     *   popupTitle: string,
     *   description: string,
     *   color: string,
     *   divs: !Array.<HTMLDivElement>,
     *   data: ?Array.<{time: number}>
     * }}
     */
    PerformanceMonitor.EventDetails;

    /**
     * Metrics fall into categories that have common units and thus may
     * share a common graph, or share y-axes within a multi-y-axis graph.
     * Each category has one home chart in which metrics of that category
     * are displayed. Currently this is also the only chart in which such
     * metrics are displayed, but the design permits a metric to show in
     * several charts if this is useful later on.
     * @type {Object.<string, {
     *   metricCategoryId: number,
     *   name: string,
     *   description: string,
     *   unit: string,
     *   details: Array.<{!PerformanceMonitor.MetricDetails}>,
     *   homeChart: !HTMLDivElement
     * }>}
     * @private
     */
    this.metricCategoryMap_ = {};

    /**
     * Comprehensive map from metricId to MetricDetails.
     * @type {Object.<number, {PerformanceMonitor.MetricDetails}>}
     * @private
     */
    this.metricDetailsMap_ = {};

    /**
     * Events fall into categories just like metrics, above. This category
     * grouping is not as important as that for metrics, since events
     * needn't share maxima, y-axes, nor units, and since events appear on
     * all charts. But grouping of event categories in the event-selection
     * UI is still useful.
     * @type {Object.<string, {
     *   eventCategoryId: number,
     *   name: string,
     *   description: string,
     *   details: !Array.<!PerformanceMonitor.EventDetails>,
     * }>}
     * @private
     */
    this.eventCategoryMap_ = {};

    /**
     * Comprehensive map from eventId to EventDetails.
     * @type {Object.<number, {PerformanceMonitor.EventDetails}>}
     * @private
     */
    this.eventDetailsMap_ = {};

    /**
     * Time periods in which the browser was active and collecting metrics
     * and events.
     * @type {!Array.<{start: number, end: number}>}
     * @private
     */
    this.intervals_ = [];

    /**
     * Handle of timer interval function used to update charts
     * @type {Object}
     * @private
     */
    this.updateTimer_ = null;

    /**
     * Handle of timer interval function used to check for resizes. Nonnull
     * only when resize events are coming steadily.
     * @type {Object}
     * @private
     */
    this.resizeTimer_ = null;

    /**
     * All chart divs in the display, whether hidden or not. Presently
     * this has one entry for each metric category in |this.metricCategoryMap|.
     * @type {Array.<HTMLDivElement>}
     * @private
     */
    this.charts_ = [];

    this.setupTimeRangeTab_();
    chrome.send('getEventTypes');
    chrome.send('getMetricTypes');
    TimeRange_.day.element.click();
  }

  PerformanceMonitor.prototype = {
    /**
     * Receive a list of all metric categories, each with its corresponding
     * list of metric details. Populate |this.metricCategoryMap_| and
     * |this.metrictDetailsMap_| to reflect said list. Reconfigure the
     * checkbox set for metric selection.
     * @param {Array.<{
     *   metricCategoryId: number,
     *   name: string,
     *   unit: string,
     *   description: string,
     *   details: Array.<{
     *     metricId: number,
     *     name: string,
     *     description: string
     *   }>
     * }>} categories  All metric categories needing charts and checkboxes.
     */
    getMetricTypesCallback: function(categories) {
      categories.forEach(function(category) {
        this.addCategoryChart_(category);
        this.metricCategoryMap_[category.metricCategoryId] = category;

        category.details.forEach(function(metric) {
          metric.color = ColorTable_[metric.metricId % ColorTable_.length];
          metric.maxValue = 1;
          metric.divs = [];
          metric.data = null;
          metric.category = category;
          this.metricDetailsMap_[metric.metricId] = metric;
        }, this);
      }, this);

      this.setupCheckboxes_($('#choose-metrics')[0],
          this.metricCategoryMap_, 'metricId', this.addMetric, this.dropMetric);
    },

    /**
     * Receive a list of all event categories, each with its correspoinding
     * list of event details. Populate |this.eventCategoryMap_| and
     * |this.eventDetailsMap| to reflect said list. Reconfigure the
     * checkbox set for event selection.
     * @param {Array.<{
     *   eventCategoryId: number,
     *   name: string,
     *   description: string,
     *   details: Array.<{
     *     eventId: number,
     *     name: string,
     *     description: string
     *   }>
     * }>} categories  All event categories needing charts checkboxes.
     */
    getEventTypesCallback: function(categories) {
      categories.forEach(function(category) {
        this.eventCategoryMap_[category.eventCategoryId] = category;

        category.details.forEach(function(event) {
          event.color = ColorTable_[event.eventId % ColorTable_.length];
          event.divs = [];
          event.data = null;
          this.eventDetailsMap_[event.eventId] = event;
        }, this);
      }, this);

      this.setupCheckboxes_($('#choose-events')[0], this.eventCategoryMap_,
          'eventId', this.addEventType, this.dropEventType);
    },

    /**
     * Set up the radio button set to choose time range. Use div#radio-template
     * as a template.
     * @private
     */
    setupTimeRangeTab_: function() {
      var timeDiv = $('#choose-time-range')[0];
      var backButton = $('#back-time')[0];
      var forwardButton = $('#forward-time')[0];
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

        this.setTimeRange(e.target.timeRange, Date.now(), true);
      }.bind(this));

      forwardButton.addEventListener('click', this.forwardTime.bind(this));
      backButton.addEventListener('click', this.backTime.bind(this));
    },

    /**
     * Generalized function to create checkboxes for either events
     * or metrics, given a |div| into which to put the checkboxes, and a
     * |optionCategoryMap| describing the checkbox structure.
     *
     * For instance, |optionCategoryMap| might be metricCategoryMap_, with
     * contents thus:
     *
     * optionCategoryMap : {
     *   1: {
     *     name: 'CPU',
     *     details: [
     *       {metricId: 1, name: 'CPU Usage', color: 'rgb(255, 128, 128)'}
     *     ],
     *   2: {
     *     name : 'Memory',
     *     details: [
     *       {metricId: 2, name: 'Private Memory Usage',
     *           color: 'rgb(128, 255, 128)'},
     *       {metricId: 3, name: 'Shared Memory Usage',
     *           color: 'rgb(128, 128, 255)'}
     *     ]
     *  }
     *
     * and we would call setupCheckboxes_ thus:
     *
     * this.setupCheckboxes_(<parent div>, this.metricCategoryMap_, 'metricId',
     *     this.addMetric, this.dropMetric);
     *
     * MetricCategoryMap_'s values each have a |name| and |details| property.
     * SetupCheckboxes_ creates one major header for each such value, with title
     * given by the |name| field. Under each major header are checkboxes,
     * one for each element in the |details| property. The checkbox titles
     * come from the |name| property of each |details| object,
     * and they each have an associated colored icon matching the |color|
     * property of the details object.
     *
     * So, for the example given, the generated HTML looks thus:
     *
     * <div id="category-label-template" class="category-label">CPU</div>
     *   <div id="detail-checkbox-template" class="detail-checkbox">
     *     <div class="horizontal-box">
     *       <input type="checkbox">
     *       <div class="detail-label">CPU Usage</div>
     *       <div class="spacer"></div>
     *       <div class="color-icon"
     *           style="background-color: rgb(255, 128, 128);"></div>
     *     </div>
     *   </div>
     * </div>
     *
     * <div id="category-label-template" class="category-label">Memory</div>
     *   <div id="detail-checkbox-template" class="detail-checkbox">
     *     <div class="horizontal-box">
     *       <input type="checkbox">
     *       <div class="detail-label">Private Memory Usage</div>
     *       <div class="spacer"></div>
     *       <div class="color-icon"
     *           style="background-color: rgb(128, 255, 128);"></div>
     *     </div>
     *   </div>
     *   </div><div id="detail-checkbox-template" class="detail-checkbox">
     *      <div class="horizontal-box">
     *        <input type="checkbox">
     *        <div class="detail-label">Shared Memory Usage</div>
     *        <div class="spacer"></div>
     *        <div class="color-icon"
     *            style="background-color: rgb(128, 128, 255);"></div>
     *     </div>
     *   </div>
     * </div>
     *
     * The checkboxes for each details object call addMetric or
     * dropMetric as they are checked and unchecked, passing the relevant
     * |metricId| value. Parameter 'metricId' identifies key |metricId| as the
     * identifying property to pass to the methods. So, for instance, checking
     * the CPU Usage box results in a call to this.addMetric(1), since
     * metricCategoryMap_[1].details[0].metricId == 1.
     *
     * In general, |optionCategoryMap| must have values that each include
     * a property |name|, and a property |details|. The |details| value must
     * be an array of objects that in turn each have an identifying property
     * with key given by parameter |idKey|, plus a property |name| and a
     * property |color|.
     *
     * @param {!HTMLDivElement} div A <div> into which to put checkboxes.
     * @param {!Object} optionCategoryMap A map of metric/event categories.
     * @param {string} idKey The key of the id property.
     * @param {!function(this:Controller, Object)} check
     *     The function to select an entry (metric or event).
     * @param {!function(this:Controller, Object)} uncheck
     *     The function to deselect an entry (metric or event).
     * @private
     */
    setupCheckboxes_: function(div, optionCategoryMap, idKey, check, uncheck) {
      var checkboxTemplate = $('#detail-checkbox-template')[0];
      var labelTemplate = $('#category-label-template')[0];

      for (var c in optionCategoryMap) {
        var category = optionCategoryMap[c];
        var label = labelTemplate.cloneNode(true);

        label.innerText = category.name;
        div.appendChild(label);

        category.details.forEach(function(details) {
          var checkbox = checkboxTemplate.cloneNode(true);
          var input = checkbox.querySelector('input');
          var label = checkbox.querySelector('.detail-label');
          var icon = checkbox.querySelector('.color-icon');

          input.option = details[idKey];
          input.icon = icon;
          input.addEventListener('change', function(e) {
            (e.target.checked ? check : uncheck).call(this, e.target.option);
            e.target.icon.style.visibility =
                e.target.checked ? 'visible' : 'hidden';
          }.bind(this));

          label.innerText = details.name;

          icon.style.backgroundColor = details.color;

          div.appendChild(checkbox);
        }, this);
      }
    },

    /**
     * Add a new chart for |category|, making it initially hidden,
     * with no metrics displayed in it.
     * @param {!Object} category  The metric category for which to create
     *     the chart. Category is a value from metricCategoryMap_.
     * @private
     */
    addCategoryChart_: function(category) {
      var chartParent = $('#charts')[0];
      var chart = document.createElement('div');

      chart.className = 'chart';
      chart.hidden = true;
      chartParent.appendChild(chart);
      this.charts_.push(chart);
      category.homeChart = chart;
      chart.refs = 0;
      chart.hovers = [];

      // Receive hover events from Flot.
      // Attached to chart will be properties 'hovers', a list of {x, div}
      // pairs. As pos events arrive, check each hover to see if it should
      // be hidden or made visible.
      $(chart).bind('plothover', function(event, pos, item) {
        var tolerance = this.range.resolution;

        chart.hovers.forEach(function(hover) {
          hover.div.hidden = hover.x < pos.x - tolerance ||
              hover.x > pos.x + tolerance;
        });

      }.bind(this));

      $(window).resize(function() {
        if (this.resizeTimer_ != null)
          clearTimeout(this.resizeTimer_);
        this.resizeTimer_ = setTimeout(this.checkResize_.bind(this),
            resizeDelay_);
      }.bind(this));
    },

    /**
     * |resizeDelay_| ms have elapsed since the last resize event, and the timer
     * for redrawing has triggered. Clear it, and redraw all the charts.
     * @private
     */
    checkResize_: function() {
      clearTimeout(this.resizeTimer_);
      this.resizeTimer_ = null;

      this.charts_.forEach(function(chart) {this.drawChart(chart);}, this);
    },

    /**
     * Set the time range for which to display metrics and events. For
     * now, the time range always ends at 'now', but future implementations
     * may allow time ranges not so anchored.
     * @param {!{start: number, end: number, resolution: number}} range
     *     The time range for which to get display data.
     * @param {number} end Ending time, in ms since epoch, to which to
     *     set the new time range.
     * @param {boolean} startTimer Indicates whether we should restart the
     *     range-update timer.
     */
    setTimeRange: function(range, end, startTimer) {
      this.range = range;
      if (this.updateTimer_ != null)
        clearInterval(this.updateTimer_);
      if (startTimer)
        this.updateTimer_ = setInterval(this.forwardTime.bind(this),
            intervalMultiple_ * range.resolution);
      this.end = Math.floor(end / range.resolution) *
          range.resolution;

      this.start = this.end - range.timeSpan;
      this.requestIntervals();
    },

    /**
     * Back up the time range by 1/2 of its current span, and cause chart
     * redraws.
     */
    backTime: function() {
      this.setTimeRange(this.range, this.end - this.range.timeSpan / 2, false);
    },

    /**
     * Advance the time range by 1/2 of its current span, or up to the point
     * where it ends at the present time, whichever is less.
     */
    forwardTime: function() {
      var now = Date.now();
      var newEnd = Math.min(now, this.end + this.range.timeSpan / 2);

      this.setTimeRange(this.range, newEnd, newEnd == now);
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

      for (var metric in this.metricDetailsMap_) {
        var metricDetails = this.metricDetailsMap_[metric];
        if (metricDetails.divs.length > 0)  // if we're displaying this metric.
          this.refreshMetric(metricDetails);
      }

      for (var eventType in this.eventDetailsMap_) {
        var eventValue = this.eventDetailsMap_[eventType];
        if (eventValue.divs.length > 0)
          this.refreshEventType(eventValue.eventId);
      }
    },

    /**
     * Add a new metric to the display, showing it on its category's home
     * chart. Un-hide the home chart if needed.
     * @param {number} metricId The id of the metric to start displaying.
     */
    addMetric: function(metricId) {
      var details = this.metricDetailsMap_[metricId];
      var homeChart = details.category.homeChart;

      details.divs.push(homeChart);
      if (homeChart.refs == 0)
        homeChart.hidden = false;
      homeChart.refs++;

      this.refreshMetric(details);
    },

    /**
     * Remove a metric from the chart(s).
     * @param {string} metric The metric to stop displaying.
     */
    dropMetric: function(metric) {
      var details = this.metricDetailsMap_[metric];
      var affectedCharts = details.divs;

      details.divs = [];  // Gotta do this now to get correct drawChart result.
      affectedCharts.forEach(function(chart) {
        chart.refs--;
        if (chart.refs == 0)
          chart.hidden = true;
        this.drawChart(chart);
      }, this);

    },

    /**
     * Request new metric data, assuming the metric table and chart already
     * exist.
     * @param {string} metricDetails The metric details for which to get data.
     */
    refreshMetric: function(metricDetails) {
      metricDetails.data = null;  // Mark metric as awaiting response.
      chrome.send('getMetric', [metricDetails.metricId,
          this.start, this.end, this.range.resolution]);
    },

    /**
     * Receive new datapoints for a metric, convert the data to Flot-usable
     * form, and redraw all affected charts.
     * @param {!{
     *   id: number,
     *   max: number,
     *   metrics: !Array.<{time: number, value: number}>
     * }} result An object giving metric ID, max expected value of the data,
     *     and time/value point pairs for that id.
     */
    getMetricCallback: function(result) {

      var metricDetails = this.metricDetailsMap_[result.metricId];
      // Might have been dropped while waiting for data.
      if (metricDetails.divs.length == 0)
        return;

      var series = [];
      metricDetails.data = [series];  // Data ends with current open series.

      // Traverse the points, and the intervals, in parallel. Both are in
      // ascending time order. Create a sequence of data 'series' (per Flot)
      // arrays, with each series comprising all points within a given interval.
      var interval = this.intervals_[0];
      var intervalIndex = 0;
      var pointIndex = 0;
      while (pointIndex < result.metrics.length &&
          intervalIndex < this.intervals_.length) {
        var point = result.metrics[pointIndex++];
        while (intervalIndex < this.intervals_.length &&
            point.time > interval.end) {
          interval = this.intervals_[++intervalIndex];  // Jump to new interval.
          if (series.length > 0) {
            series = [];  // Start a new series.
            metricDetails.data.push(series);  // Put it on the end of the data.
          }
        }
        if (intervalIndex < this.intervals_.length &&
            point.time > interval.start) {
          series.push([point.time - timezoneOffset_, point.value]);
        }
      }
      metricDetails.maxValue = Math.max(metricDetails.maxValue,
          result.maxValue);

      metricDetails.divs.forEach(this.drawChart, this);
    },

    /**
     * Add a new event to the chart(s).
     * @param {string} eventType The type of event to start displaying.
     */
    addEventType: function(eventType) {
      // Events show on all charts.
      this.eventDetailsMap_[eventType].divs = this.charts_;
      this.refreshEventType(eventType);
    },

    /*
     * Remove an event from the chart(s).
     * @param {string} eventType The type of event to stop displaying.
     */
    dropEventType: function(eventType) {
      var eventValue = this.eventDetailsMap_[eventType];
      var affectedCharts = eventValue.divs;

      eventValue.divs = [];  // Gotta do this now for correct drawChart results.
      affectedCharts.forEach(this.drawChart, this);
    },

    /**
     * Request new data for |eventType|, for times in the current range.
     * @param {string} eventType The type of event to get new data for.
     */
    refreshEventType: function(eventType) {
      // Mark eventType as awaiting response.
      this.eventDetailsMap_[eventType].data = null;

      chrome.send('getEvents', [eventType, this.start, this.end]);
    },

    /**
     * Receive new events for |eventType|. If |eventType| has been deselected
     * while awaiting webui handler response, do nothing. Otherwise, save the
     * data directly, since events are handled differently than metrics
     * when drawing (no 'series'), and redraw all the affected charts.
     * @param {!{
     *   id: number,
     *   events: !Array.<{time: number}>
     * }} result An object giving eventType id, and times at which that event
     *     type occurred in the range requested. Each object in the array may
     *     also have an arbitrary list of properties to be displayed as
     *     a tooltip message for the event.
     */
    getEventsCallback: function(result) {
      var eventValue = this.eventDetailsMap_[result.eventId];

      if (eventValue.divs.length == 0)
        return;

      result.events.forEach(function(element) {
        element.time -= timezoneOffset_;
      });

      eventValue.data = result.events;
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

      for (var metric in this.metricDetailsMap_) {
        var metricDetails = this.metricDetailsMap_[metric];

        if (metricDetails.divs.indexOf(chart) != -1)
          result.metrics.push(metricDetails);
      }

      for (var eventType in this.eventDetailsMap_) {
        var eventValue = this.eventDetailsMap_[eventType];

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
      return chartData.metrics.every(function(metric) {return metric.data;}) &&
          chartData.events.every(function(event) {return event.data;});
    },

    /**
     * Create and return an array of 'markings' (per Flot), representing
     * vertical lines at the event time, in the event's color. Also add
     * (not per Flot) a |popupTitle| property to each, to be used for
     * labelling description popups.
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
      var explanation;
      var date, hours, minutes;

      eventValues.forEach(function(eventValue) {
        eventValue.data.forEach(function(point) {
          if (point.time >= this.start - timezoneOffset_ &&
              point.time <= this.end - timezoneOffset_) {

            // Date wants Zulu time.
            date = new Date(point.time + timezoneOffset_);
            hours = date.getHours();
            minutes = date.getMinutes();
            explanation = eventValue.popupTitle + '\nAt: ' +
                (date.getMonth() + 1) + '/' + date.getDate() + ' ' +
                (hours < 10 ? '0' : '') + hours + ':' +
                (minutes < 10 ? '0' : '') + minutes + '\n';

            for (var key in point) {
              if (key != 'time') {
                var datum = point[key];
                if ('label' in datum && 'value' in datum)
                  explanation = explanation + datum.label + ': ' +
                      datum.value + '\n';
              }
            }
            markings.push({
              color: eventValue.color,
              popupContent: explanation,
              xaxis: {from: point.time, to: point.time}
            });
          } else {
            console.log('Event out of time range ' + this.start + ' -> ' +
                this.end + ' at: ' + point.time);
          }
        }, this);
      }, this);

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
      var axisMap = {}; // Maps category ids to y-axis numbers

      if (chart.hidden || !this.isDataReady_(chartData))
        return;

      var seriesSeq = [];
      var yAxes = [];
      chartData.metrics.forEach(function(metricDetails) {
        var metricCategory = metricDetails.category;
        var yAxisNumber = axisMap[metricCategory.metricCategoryId];

        // Add a new y-axis if we are encountering this category of metric
        // for the first time. Otherwise, update the existing y-axis with
        // a new max value if needed. (Presently, we expect only one category
        // of metric per chart, but this design permits more in the future.)
        if (yAxisNumber === undefined) {
          yAxes.push({min: 0, max: metricDetails.maxValue * yAxisMargin_,
              labelWidth: 60});
          axisMap[metricCategory.metricCategoryId] = yAxisNumber = yAxes.length;
        } else {
          yAxes[yAxisNumber - 1].max = Math.max(yAxes[yAxisNumber - 1].max,
              metricDetails.maxValue * yAxisMargin_);
        }

        for (var i = 0; i < metricDetails.data.length; i++) {
          seriesSeq.push({
            color: metricDetails.color,
            data: metricDetails.data[i],
            label: i == 0 ? metricDetails.name +
                ' (' + metricCategory.unit + ')' : null,
            yaxis: yAxisNumber
          });
        }
      });

      var markings = this.getEventMarks_(chartData.events);
      var plot = $.plot(chart, seriesSeq, {
        yaxes: yAxes,
        xaxis: {
          mode: 'time',
          min: this.start - timezoneOffset_,
          max: this.end - timezoneOffset_
        },
        points: {show: true, radius: 1},
        lines: {show: true},
        grid: {markings: markings, hoverable: true, autoHighlight: true}
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
        labelDiv.innerText = mark.popupContent;
        labelDiv.style.left = point.left + 'px';
        labelDiv.style.top = (point.top + 100 * (i % 3)) + 'px';

        chart.appendChild(labelDiv);
        labelDiv.hidden = true;
        chart.hovers.push({x: mark.xaxis.to, div: labelDiv});
      }
    }
  };
  return {
    PerformanceMonitor: PerformanceMonitor
  };
});

var PerformanceMonitor = new performance_monitor.PerformanceMonitor();
