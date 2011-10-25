// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {
  'use strict';

  // A set of parameter names. An entry of 'abc' allows metrics to specify
  // events with specific values of 'abc'.
  var metricProperties = {
    'pipeline_state': true,
  };

  // A set of metrics to measure. The user will see the most recent and average
  // measurement of the time between each metric's start and end events.
  var metrics = {
    'seek': {
      'start': 'SEEK',
      'end': 'pipeline_state=started'
    },
    'first frame': {
      'start': 'WEBMEDIAPLAYER_CREATED',
      'end': 'pipeline_state=started'
    },
  };

  /**
   * This class measures times between the events specified above. It inherits
   * <li> and contains a table that displays the measurements.
   */
  var Metrics = cr.ui.define('li');

  Metrics.prototype = {
    __proto__: HTMLLIElement.prototype,

    /**
     * Decorate this <li> as a Metrics.
     */
    decorate: function() {
      this.table_ = document.createElement('table');
      var details = document.createElement('details');
      var summary = media.makeElement('summary', 'Metrics:');
      details.appendChild(summary);
      details.appendChild(this.table_);
      this.appendChild(details);

      var hRow = document.createElement('tr');
      hRow.appendChild(media.makeElement('th', 'Metric:'));
      hRow.appendChild(media.makeElement('th', 'Last Measure:'));
      hRow.appendChild(media.makeElement('th', 'Average:'));
      var header = document.createElement('thead');
      header.appendChild(hRow);
      this.table_.appendChild(header);

      for (var metric in metrics) {
        var last = document.createElement('td');
        var avg = document.createElement('td');
        this[metric] = {
          count: 0,
          total: 0,
          start: null,
          last: last,
          avg: avg
        };
        var row = document.createElement('tr');
        row.appendChild(media.makeElement('td', metric + ':'));
        row.appendChild(last);
        row.appendChild(avg);
        this.table_.appendChild(row);
      }
    },

    /**
     * An event has occurred. Update any metrics that refer to this type
     * of event. Can be called multiple times by addEvent below if the metrics
     * refer to specific parameters.
     * @param {Object} event The MediaLogEvent that has occurred.
     * @param {string} type The type of event.
     */
    addEventInternal: function(event, type) {
      var timeInMs = event.time * 1000;  // Work with milliseconds.

      for (var metric in metrics) {
        var m = this[metric];
        if (type == metrics[metric].start && !m.start) {
          m.start = timeInMs;
        } else if (type == metrics[metric].end && m.start != null) {
          var last = timeInMs - m.start;
          m.last.textContent = last.toFixed(1);
          m.total += last;
          m.count++;
          if (m.count > 1)
            m.avg.textContent = (m.total / m.count).toFixed(1);
          m.start = null;
        }
      }
    },

    /**
     * An event has occurred. Update any metrics that refer to events of this
     * type or with this event's parameters.
     * @param {Object} event The MediaLogEvent that has occurred.
     */
    addEvent: function(event) {
      this.addEventInternal(event, event.type);
      for (var p in event.params) {
        if (p in metricProperties) {
          var type = p + '=' + event.params[p];
          this.addEventInternal(event, type);
        }
      }
    },
  };

  return {
      Metrics: Metrics,
  };
});
