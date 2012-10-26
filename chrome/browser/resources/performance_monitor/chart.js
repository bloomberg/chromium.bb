/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('performance_monitor', function() {
  'use strict';

  /**
   * Array of available time resolutions.
   * @type {Array.<PerformanceMonitor.TimeResolution>}
   * @private
   */
  var TimeResolutions_ = {
    // Prior 15 min, resolution of 5s, at most 180 points.
    // Labels at 12 point (1 min) intervals.
    minutes: {id: 0, i18nKey: 'timeLastFifteenMinutes', timeSpan: 900 * 1000,
              pointResolution: 1000 * 5, labelEvery: 12},

    // Prior hour, resolution of 20s, at most 180 points.
    // Labels at 15 point (5 min) intervals.
    hour: {id: 1, i18nKey: 'timeLastHour', timeSpan: 3600 * 1000,
           pointResolution: 1000 * 20, labelEvery: 15},

    // Prior day, resolution of 5 min, at most 288 points.
    // Labels at 36 point (3 hour) intervals.
    day: {id: 2, i18nKey: 'timeLastDay', timeSpan: 24 * 3600 * 1000,
          pointResolution: 1000 * 60 * 5, labelEvery: 36},

    // Prior week, resolution of 1 hr -- at most 168 data points.
    // Labels at 24 point (daily) intervals.
    week: {id: 3, i18nKey: 'timeLastWeek', timeSpan: 7 * 24 * 3600 * 1000,
           pointResolution: 1000 * 3600, labelEvery: 24},

    // Prior month (30 days), resolution of 4 hr -- at most 180 data points.
    // Labels at 42 point (weekly) intervals.
    month: {id: 4, i18nKey: 'timeLastMonth', timeSpan: 30 * 24 * 3600 * 1000,
            pointResolution: 1000 * 3600 * 4, labelEvery: 42},

    // Prior quarter (90 days), resolution of 12 hr -- at most 180 data points.
    // Labels at 28 point (fortnightly) intervals.
    quarter: {id: 5, i18nKey: 'timeLastQuarter',
              timeSpan: 90 * 24 * 3600 * 1000,
              pointResolution: 1000 * 3600 * 12, labelEvery: 28},
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

  /*
   * The value of the 'No Aggregation' option enum (AGGREGATION_METHOD_NONE) on
   * the C++ side. We use this to warn the user that selecting this aggregation
   * option will be slow.
   */
  var aggregationMethodNone = 0;

  /*
   * The value of the default aggregation option, 'Median Aggregation'
   * (AGGREGATION_METHOD_MEDIAN), on the C++ side.
   */
  var aggregationMethodMedian = 1;

  /** @constructor */
  function PerformanceMonitor() {
    this.__proto__ = PerformanceMonitor.prototype;

    /** Information regarding a certain time resolution option, including an
     *  enumerative id, a readable name, the timespan in milliseconds prior to
     *  |now|, data point resolution in milliseconds, and time-label frequency
     *  in data points per label.
     *  @typedef {{
     *    id: number,
     *    name: string,
     *    timeSpan: number,
     *    pointResolution: number,
     *    labelEvery: number,
     *  }}
     */
    PerformanceMonitor.TimeResolution;

    /**
     * Information regarding a certain time resolution option, including an
     * enumerative id, the key to the i18n name and the name itself, the
     * timespan in milliseconds prior to |now|, data point resolution in
     * milliseconds, and time-label frequency in data points per label.
     * @typedef {{
     *   id: number,
     *   i18nKey: string,
     *   name: string
     *   timeSpan: number,
     *   pointResolution: number,
     *   labelEvery: number,
     * }}
     */
    PerformanceMonitor.TimeResolution;

    /**
     * Detailed information on a metric in the UI. MetricId is a unique
     * identifying number for the metric, provided by the webui, and assumed
     * to be densely populated. All metrics also have a description and
     * an associated category giving their unit information and home chart.
     * They also have a color in which they are displayed, and a maximum value
     * by which to scale their y-axis. Additionally, there is a corresponding
     * checkbox element, which is the checkbox used to enable or disable the
     * metric display.
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
     *   checkbox: HTMLElement,
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
     *   checkbox: HTMLElement,
     *   divs: !Array.<HTMLDivElement>,
     *   data: ?Array.<{time: number}>
     * }}
     */
    PerformanceMonitor.EventDetails;

    /**
     * The time range which we are currently viewing, with the start and end of
     * the range, as well as the resolution.
     * @type {{
     *   start: number,
     *   end: number,
     *   resolution: PerformanceMonitor.TimeResolution
     * }}
     * @private
     */
    this.range_ = { 'start': 0, 'end': 0, 'resolution': undefined };

    /**
     * The map containing the available TimeResolutions and the radio button to
     * which each corresponds. The key is the id field from the TimeResolution
     * object.
     * @type {Object.<string, {
     *   option: PerformanceMonitor.TimeResolution,
     *   element: HTMLElement
     * }>}
     * @private
     */
    this.timeResolutionRadioMap_ = {};

    /**
     * The map containing the available Aggregation Methods and the radio button
     * to which each corresponds. The different methods are retrieved from the
     * WebUI, and the information about the method is stored in the 'option'
     * field. The key to the map is the id of the aggregation method.
     * @type {Object.<string, {
     *   option: {
     *     id: number,
     *     name: string,
     *     description: string,
     *   },
     *   element: HTMLElement
     * }>}
     * @private
     */
    this.aggregationRadioMap_ = {};

    /**
     * Metrics fall into categories that have common units and thus may
     * share a common graph, or share y-axes within a multi-y-axis graph.
     * Each category has one home chart in which metrics of that category
     * are displayed. Currently this is also the only chart in which such
     * metrics are displayed, but the design permits a metric to show in
     * several charts if this is useful later on. The key is metricCategoryId.
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
     * @type {Object.<string, {PerformanceMonitor.MetricDetails}>}
     * @private
     */
    this.metricDetailsMap_ = {};

    /**
     * Events fall into categories just like metrics, above. This category
     * grouping is not as important as that for metrics, since events
     * needn't share maxima, y-axes, nor units, and since events appear on
     * all charts. But grouping of event categories in the event-selection
     * UI is still useful. The key is the id of the event category.
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
     * @type {Object.<string, {PerformanceMonitor.EventDetails}>}
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
     * The record of all the warnings which are currently active (or empty if no
     * warnings are being displayed).
     * @type {!Array.<string>}
     * @private
     */
    this.activeWarnings_ = [];

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

    this.setupStaticControlPanelFeatures_();
    chrome.send('getFlagEnabled');
    chrome.send('getAggregationTypes');
    chrome.send('getEventTypes');
    chrome.send('getMetricTypes');
  }

  PerformanceMonitor.prototype = {
    /**
     * Display the appropriate warning at the top of the page.
     * @param {string} warningId the id of the HTML element with the warning
     *     to display; this does not include the '#'.
     */
    showWarning: function(warningId) {
      if (this.activeWarnings_.indexOf(warningId) != -1)
        return;

      if (this.activeWarnings_.length == 0)
        $('#warnings-box')[0].style.display = 'block';
      $('#' + warningId)[0].style.display = 'block';
      this.activeWarnings_.push(warningId);
    },

    /**
     * Hide the warning, and, if that was the only warning showing, the entire
     * warnings box.
     * @param {string} warningId the id of the HTML element with the warning
     *     to display; this does not include the '#'.
     */
    hideWarning: function(warningId) {
      if ($('#' + warningId)[0].style.display == 'none')
        return;
      $('#' + warningId)[0].style.display = 'none';
      this.activeWarnings_.splice(this.activeWarnings_.indexOf(warningId), 1);

      if (this.activeWarnings_.length == 0)
        $('#warnings-box')[0].style.display = 'none';
    },

    /**
     * Receive an indication of whether or not the kPerformanceMonitorGathering
     * flag has been enabled and, if not, warn the user of such.
     * @param {boolean} flagEnabled indicates whether or not the flag has been
     *     enabled.
     */
    getFlagEnabledCallback: function(flagEnabled) {
      if (!flagEnabled)
        this.showWarning('flag-not-enabled-warning');
    },

    /**
     * Receive a list of all the aggregation methods. Populate
     * |this.aggregationRadioMap_| to reflect said list. Create the section of
     * radio buttons for the aggregation methods, and choose the first method
     * by default.
     * @param {Array<{
     *   id: number,
     *   name: string,
     *   description: string
     * }>} strategies All aggregation strategies needing radio buttons.
     */
    getAggregationTypesCallback: function(strategies) {
      strategies.forEach(function(strategy) {
        this.aggregationRadioMap_[strategy.id] = { 'option': strategy };
      }, this);

      this.setupRadioButtons_($('#choose-aggregation')[0],
                              this.aggregationRadioMap_,
                              this.setAggregationStrategy,
                              aggregationMethodMedian,
                              'aggregation-strategies');
      this.setAggregationStrategy(aggregationMethodMedian);
    },

    /**
     * Receive a list of all metric categories, each with its corresponding
     * list of metric details. Populate |this.metricCategoryMap_| and
     * |this.metricDetailsMap_| to reflect said list. Reconfigure the
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
     * }>} categories All metric categories needing charts and checkboxes.
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

      for (var metric in this.metricDetailsMap_)
        this.metricDetailsMap_[metric].checkbox.click();
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
     * }>} categories All event categories needing charts and checkboxes.
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
     * Set up the aspects of the control panel which are not dependent upon the
     * information retrieved from PerformanceMonitor's database; this includes
     * the Time Resolutions and Aggregation Methods radio sections.
     * @private
     */
    setupStaticControlPanelFeatures_: function() {
      // Initialize the options in the |timeResolutionRadioMap_| and set the
      // localized names for the time resolutions.
      for (var key in TimeResolutions_) {
        var resolution = TimeResolutions_[key];
        this.timeResolutionRadioMap_[resolution.id] = { 'option': resolution };
        resolution.name = loadTimeData.getString(resolution.i18nKey);
      }

      // Setup the Time Resolution radio buttons, and select the default option
      // of minutes (finer resolution in order to ensure that the user sees
      // something at startup).
      this.setupRadioButtons_($('#choose-time-range')[0],
                              this.timeResolutionRadioMap_,
                              this.changeTimeResolution_,
                              TimeResolutions_.minutes.id,
                              'time-resolutions');

      // Set the default selection to 'Minutes' and set the time range.
      this.setTimeRange(TimeResolutions_.minutes,
                        Date.now(),
                        true);  // Auto-refresh the chart.

      var forwardButton = $('#forward-time')[0];
      forwardButton.addEventListener('click', this.forwardTime.bind(this));
      var backButton = $('#back-time')[0];
      backButton.addEventListener('click', this.backTime.bind(this));
    },

    /**
     * Change the current time resolution. The visible range will stay centered
     * around the current center unless the latest edge crosses now(), in which
     * case it will be pinned there and start auto-updating.
     * @param {number} mapId the index into the |timeResolutionRadioMap_| of the
     *     selected resolution.
     */
    changeTimeResolution_: function(mapId) {
      var newEnd;
      var now = Date.now();
      var newResolution = this.timeResolutionRadioMap_[mapId].option;

      // If we are updating the timer, then we know that we are already ending
      // at the perceived current time (which may be different than the actual
      // current time, since we don't update continuously).
      newEnd = this.updateTimer_ ? now :
          Math.min(now, this.range_.end + (newResolution.timeSpan -
              this.range_.resolution.timeSpan) / 2);

      this.setTimeRange(newResolution, newEnd, newEnd == now);
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
     *       {
     *         metricId: 1,
     *         name: 'CPU Usage',
     *         description:
     *             'The combined CPU usage of all processes related to Chrome',
     *         color: 'rgb(255, 128, 128)'
     *       }
     *     ],
     *   2: {
     *     name : 'Memory',
     *     details: [
     *       {
     *         metricId: 2,
     *         name: 'Private Memory Usage',
     *         description:
     *             'The combined private memory usage of all processes related
     *             to Chrome',
               color: 'rgb(128, 255, 128)'
     *       },
     *       {
     *         metricId: 3,
     *         name: 'Shared Memory Usage',
     *         description:
     *             'The combined shared memory usage of all processes related
     *             to Chrome',
               color: 'rgb(128, 128, 255)'
     *       }
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
     * <div>
     *   <h3 class="category-heading">CPU</h3>
     *   <div class="checkbox-group">
     *     <div>
     *       <label class="input-label" title=
     *           "The combined CPU usage of all processes related to Chrome">
     *         <input type="checkbox">
     *         <span>CPU</span>
     *       </label>
     *     </div>
     *   </div>
     * </div>
     * <div>
     *   <h3 class="category-heading">Memory</h3>
     *   <div class="checkbox-group">
     *     <div>
     *       <label class="input-label" title= "The combined private memory \
     *           usage of all processes related to Chrome">
     *         <input type="checkbox">
     *         <span>Private Memory</span>
     *       </label>
     *     </div>
     *     <div>
     *       <label class="input-label" title= "The combined shared memory \
     *           usage of all processes related to Chrome">
     *         <input type="checkbox">
     *         <span>Shared Memory</span>
     *       </label>
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
      var categoryTemplate = $('#category-template')[0];
      var checkboxTemplate = $('#checkbox-template')[0];

      for (var c in optionCategoryMap) {
        var category = optionCategoryMap[c];
        var template = categoryTemplate.cloneNode(true);
        template.id = '';

        var heading = template.querySelector('.category-heading');
        heading.innerText = category.name;
        heading.title = category.description;

        var checkboxGroup = template.querySelector('.checkbox-group');
        category.details.forEach(function(details) {
          var checkbox = checkboxTemplate.cloneNode(true);
          checkbox.id = '';
          var input = checkbox.querySelector('input');

          details.checkbox = input;
          input.checked = false;
          input.option = details[idKey];
          input.addEventListener('change', function(e) {
            (e.target.checked ? check : uncheck).call(this, e.target.option);
          }.bind(this));

          checkbox.querySelector('span').innerText = details.name;
          checkbox.querySelector('.input-label').title = details.description;

          checkboxGroup.appendChild(checkbox);
        }, this);

        div.appendChild(template);
      }
    },

    /**
     * Generalized function to create radio buttons in a collection of
     * |collectionName|, given a |div| into which the radio buttons are placed
     * and a |optionMap| describing the radio buttons' options.
     *
     * optionMaps have two guaranteed fields - 'option' and 'element'. The
     * 'option' field corresponds to the item which the radio button will be
     * representing (e.g., a particular aggregation strategy).
     *   - Each 'option' is guaranteed to have a 'value', a 'name', and a
     *     'description'. 'Value' holds the id of the option, while 'name' and
     *     'description' are internationalized strings for the radio button's
     *     content.
     *   - 'Element' is the field devoted to the HTMLElement for the radio
     *     button corresponding to that entry; this will be set in this
     *     function.
     *
     * Assume that |optionMap| is |aggregationRadioMap_|, as follows:
     * optionMap: {
     *   0: {
     *     option: {
     *       id: 0
     *       name: 'Median'
     *       description: 'Aggregate using median calculations to reduce
     *           noisiness in reporting'
     *     },
     *     element: null
     *   },
     *   1: {
     *     option: {
     *       id: 1
     *       name: 'Mean'
     *       description: 'Aggregate using mean calculations for the most
     *           accurate average in reporting'
     *     },
     *     element: null
     *   }
     * }
     *
     * and we would call setupRadioButtons_ with:
     * this.setupRadioButtons_(<parent_div>, this.aggregationRadioMap_,
     *     this.setAggregationStrategy, 0, 'aggregation-strategies');
     *
     * The resultant HTML would be:
     * <div class="radio">
     *   <label class="input-label" title="Aggregate using median \
     *       calculations to reduce noisiness in reporting">
     *     <input type="radio" name="aggregation-strategies" value=0>
     *     <span>Median</span>
     *   </label>
     * </div>
     * <div class="radio">
     *   <label class="input-label" title="Aggregate using mean \
     *       calculations for the most accurate average in reporting">
     *     <input type="radio" name="aggregation-strategies" value=1>
     *     <span>Mean</span>
     *   </label>
     * </div>
     *
     * If a radio button is selected, |onSelect| is called with the radio
     * button's value. The |defaultKey| is used to choose which radio button
     * to select at startup; the |onSelect| method is not called on this
     * selection.
     *
     * @param {!HTMLDivElement} div A <div> into which we place the radios.
     * @param {!Object} optionMap A map containing the radio button information.
     * @param {!function(this:Controller, Object)} onSelect
     *     The function called when a radio is selected.
     * @param {string} defaultKey The key to the radio which should be selected
     *     initially.
     * @param {string} collectionName The name of the radio button collection.
     * @private
     */
    setupRadioButtons_: function(div,
                                 optionMap,
                                 onSelect,
                                 defaultKey,
                                 collectionName) {
      var radioTemplate = $('#radio-template')[0];
      for (var key in optionMap) {
        var entry = optionMap[key];
        var radio = radioTemplate.cloneNode(true);
        radio.id = '';
        var input = radio.querySelector('input');

        input.name = collectionName;
        input.enumerator = entry.option.id;
        input.option = entry;
        radio.querySelector('span').innerText = entry.option.name;
        if (entry.option.description != undefined)
          radio.querySelector('.input-label').title = entry.option.description;
        div.appendChild(radio);
        entry.element = input;
      }

      optionMap[defaultKey].element.click();

      div.addEventListener('click', function(e) {
        if (!e.target.webkitMatchesSelector('input[type="radio"]'))
          return;

        onSelect.call(this, e.target.enumerator);
      }.bind(this));
    },

    /**
     * Add a new chart for |category|, making it initially hidden,
     * with no metrics displayed in it.
     * @param {!Object} category The metric category for which to create
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
        var tolerance = this.range_.resolution.pointResolution;

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
     * @param {TimeResolution} resolution
     *     The time resolution at which to display the data.
     * @param {number} end Ending time, in ms since epoch, to which to
     *     set the new time range.
     * @param {boolean} autoRefresh Indicates whether we should restart the
     *     range-update timer.
     */
    setTimeRange: function(resolution, end, autoRefresh) {
      // If we have a timer and we are no longer updating, or if we need a timer
      // for a different resolution, disable the current timer.
      if (this.updateTimer_ &&
              (this.range_.resolution != resolution || !autoRefresh)) {
        clearInterval(this.updateTimer_);
        this.updateTimer_ = null;
      }

      if (autoRefresh && !this.updateTimer_) {
        this.updateTimer_ = setInterval(
            this.forwardTime.bind(this),
            intervalMultiple_ * resolution.pointResolution);
      }

      this.range_.resolution = resolution;
      this.range_.end = Math.floor(end / resolution.pointResolution) *
          resolution.pointResolution;
      this.range_.start = this.range_.end - resolution.timeSpan;

      this.requestIntervals();
    },

    /**
     * Back up the time range by 1/2 of its current span, and cause chart
     * redraws.
     */
    backTime: function() {
      this.setTimeRange(this.range_.resolution,
                        this.range_.end - this.range_.resolution.timeSpan / 2,
                        false);
    },

    /**
     * Advance the time range by 1/2 of its current span, or up to the point
     * where it ends at the present time, whichever is less.
     */
    forwardTime: function() {
      var now = Date.now();
      var newEnd =
          Math.min(now, this.range_.end + this.range_.resolution.timeSpan / 2);

      this.setTimeRange(this.range_.resolution, newEnd, newEnd == now);
    },

    /**
     * Request activity intervals in the current time range.
     */
    requestIntervals: function() {
      chrome.send('getActiveIntervals', [this.range_.start, this.range_.end]);
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
     * Set the aggregation strategy.
     * @param {number} strategyId The id of the aggregation strategy.
     */
    setAggregationStrategy: function(strategyId) {
      if (strategyId != aggregationMethodNone)
        this.hideWarning('no-aggregation-warning');
      else
        this.showWarning('no-aggregation-warning');

      this.aggregationStrategy = strategyId;
      this.requestIntervals();
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
          this.range_.start, this.range_.end,
          this.range_.resolution.pointResolution,
          this.aggregationStrategy]);
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

      chrome.send('getEvents', [eventType, this.range_.start, this.range_.end]);
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
          if (point.time >= this.range_.start - timezoneOffset_ &&
              point.time <= this.range_.end - timezoneOffset_) {

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
            console.log('Event out of time range ' + this.range_.start +
                ' -> ' + this.range_.end + ' at: ' + point.time);
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
          min: this.range_.start - timezoneOffset_,
          max: this.range_.end - timezoneOffset_
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
