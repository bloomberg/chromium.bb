// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../../../../third_party/polymer_legacy/platform/platform.js">
<include src="../../../../third_party/polymer_legacy/polymer/polymer.js">

/**
 * Formats size to a human readable form.
 * @param {number} size Size in bytes.
 * @return {string} Output string in a human-readable format.
 */
function formatSizeCommon(size) {
  if (size < 1024)
    return size + ' B';
  if (size < 1024 * 1024)
    return Math.round(size / 1024) + ' KB';
  return Math.round(size / 1024 / 1024) + ' MB';
}

// Defines the file-systems element.
Polymer('file-systems', {
  /**
   * Called when the element is created.
   */
  ready: function() {
  },

  /**
   * Selects an active file system from the list.
   * @param {Event} event Event.
   * @param {number} detail Detail.
   * @param {HTMLElement} sender Sender.
   */
  rowClicked: function(event, detail, sender) {
    var requestEventsNode = document.querySelector('#request-events');
    requestEventsNode.hidden = false;
    requestEventsNode.model = [];

    var requestTimelineNode = document.querySelector('#request-timeline');
    requestTimelineNode.hidden = false;
    requestTimelineNode.model = [];

    chrome.send('selectFileSystem', [sender.dataset.extensionId,
      sender.dataset.id]);
  },

  /**
   * List of provided file system information maps.
   * @type {Array.<Object>}
   */
  model: []
});

// Defines the request-log element.
Polymer('request-events', {
  /**
   * Called when the element is created.
   */
  ready: function() {
  },

  /**
   * Formats time to a hh:mm:ss.xxxx format.
   * @param {Date} time Input time.
   * @return {string} Output string in a human-readable format.
   */
  formatTime: function(time) {
    return ('0' + time.getHours()).slice(-2) + ':' +
           ('0' + time.getMinutes()).slice(-2) + ':' +
           ('0' + time.getSeconds()).slice(-2) + '.' +
           ('000' + time.getMilliseconds()).slice(-3);
  },

  /**
   * Formats size to a human readable form.
   * @param {number} size Size in bytes.
   * @return {string} Output string in a human-readable format.
   */
  formatSize: function(size) {
    return formatSizeCommon(size);
  },

  /**
   * Formats a boolean value to human-readable form.
   * @param {boolean=} opt_hasMore Input value.
   * @return {string} Output string in a human-readable format.
   */
  formatHasMore: function(opt_hasMore) {
    if (opt_hasMore == undefined)
      return '';

    return opt_hasMore ? 'HAS_MORE' : 'LAST';
  },

  /**
   * Formats execution time to human-readable form.
   * @param {boolean=} opt_executionTime Input value.
   * @return {string} Output string in a human-readable format.
   */
  formatExecutionTime: function(opt_executionTime) {
    if (opt_executionTime == undefined)
      return '';

    return opt_executionTime + ' ms';
  },

  /**
   * List of events.
   * @type {Array.<Object>}
   */
  model: []
});

// Defines the request-timeline element.
Polymer('request-timeline', {
  /**
   * Step for zoomin in and out.
   * @type {number}
   * @const
   */
  SCALE_STEP: 1.5,

  /**
   * Height of each row in the chart in pixels.
   * @type {number}
   * @const
   */
  ROW_HEIGHT: 14,

  /**
   * Observes changes in the model.
   * @type {Object.<string, string>}
   */
  observe: {
    'model.length': 'chartUpdate'
  },

  /**
   * Called when the element is created.
   */
  ready: function() {
    // Update active requests in the background for nice animation.
    var activeUpdateAnimation = function() {
      this.activeUpdate();
      requestAnimationFrame(activeUpdateAnimation);
    }.bind(this);
    activeUpdateAnimation();
  },

  /**
   * Formats size to a human readable form.
   * @param {number} size Size in bytes.
   * @return {string} Output string in a human-readable format.
   */
  formatSize: function(size) {
    return formatSizeCommon(size);
  },

  /**
   * Zooms in the timeline.
   * @param {Event} event Event.
   * @param {number} detail Detail.
   * @param {HTMLElement} sender Sender.
   */
  zoomInClicked: function(event, detail, sender) {
    this.scale *= this.SCALE_STEP;
  },

  /**
   * Zooms out the timeline.
   * @param {Event} event Event.
   * @param {number} detail Detail.
   * @param {HTMLElement} sender Sender.
   */
  zoomOutClicked: function(event, detail, sender) {
    this.scale /= this.SCALE_STEP;
  },

  /**
   * Selects or deselects an element on the timeline.
   * @param {Event} event Event.
   * @param {number} detail Detail.
   * @param {HTMLElement} sender Sender.
   */
  elementClicked: function(event, detail, sender) {
    if (sender.dataset.id in this.selected) {
      delete this.selected[sender.dataset.id];
      sender.classList.remove('selected');
    } else {
      this.selected[sender.dataset.id] = true;
      sender.classList.add('selected');
    }

    var requestEventsNode = document.querySelector('#request-events');
    requestEventsNode.hidden = false;

    requestEventsNode.model = [];
    for (var i = 0; i < this.model.length; i++) {
      if (this.model[i].id in this.selected)
        requestEventsNode.model.push(this.model[i]);
    }
  },

  /**
   * Updates chart elements of active requests, so they grow with time.
   */
  activeUpdate: function() {
    if (Object.keys(this.active).length == 0)
      return;

    for (var id in this.active) {
      var index = this.active[id];
      this.chart[index].length = Date.now() - this.chart[index].time;
    }
  },

  /**
   * Generates <code>chart</code> from the new <code>model</code> value.
   */
  chartUpdate: function(oldLength, newLength) {
    // If the new value is empty, then clear the model.
    if (!newLength) {
      this.active = {};
      this.rows = [];
      this.chart = [];
      this.timeStart = null;
      this.idleStart = null;
      this.idleTotal = 0;
      this.selected = [];
      return;
    }

    // Only adding new entries to the model is supported (or clearing).
    console.assert(newLength >= oldLength);

    for (var i = oldLength; i < newLength; i++) {
      var event = this.model[i];
      switch (event.eventType) {
        case 'created':
          // If this is the first creation event in the chart, then store its
          // time as beginning time of the chart.
          if (!this.timeStart)
            this.timeStart = event.time;

          // If this event terminates idling, then add the idling time to total
          // idling time. This is used to avoid gaps in the chart while idling.
          if (Object.keys(this.active).length == 0 && this.idleStart)
            this.idleTotal += event.time.getTime() - this.idleStart.getTime();

          // Find the appropriate row for this chart element.
          var rowIndex = 0;
          while (true) {
            // Add to this row only if there is enough space, and if the row
            // is of the same type.
            var addToRow = (rowIndex >= this.rows.length) ||
                (this.rows[rowIndex].time.getTime() <= event.time.getTime() &&
                 !this.rows[rowIndex].active &&
                 (this.rows[rowIndex].requestType == event.requestType));

            if (addToRow) {
              this.chart.push({
                index: this.chart.length,
                id: event.id,
                time: event.time,
                executionTime: 0,
                length: 0,
                requestType: event.requestType,
                left: event.time - this.timeStart - this.idleTotal,
                row: rowIndex,
                modelIndexes: [i]
              });

              this.rows[rowIndex] = {
                requestType: event.requestType,
                time: event.time,
                active: true
              };

              this.active[event.id] = this.chart.length - 1;
              break;
            }

            rowIndex++;
          }
          break;

        case 'fulfilled':
        case 'rejected':
          if (!(event.id in this.active))
            return;
          var chartIndex = this.active[event.id];
          this.chart[chartIndex].state = event.eventType;
          this.chart[chartIndex].executionTime = event.executionTime;
          this.chart[chartIndex].valueSize = event.valueSize;
          this.chart[chartIndex].modelIndexes.push(i);
          break;

        case 'destroyed':
          if (!(event.id in this.active))
            return;

          var chartIndex = this.active[event.id];
          this.chart[chartIndex].length =
              event.time - this.chart[chartIndex].time;
          this.chart[chartIndex].modelIndexes.push(i);
          this.rows[this.chart[chartIndex].row].time = event.time;
          this.rows[this.chart[chartIndex].row].active = false;
          delete this.active[event.id];

          // If this was the last active request, then idling starts.
          if (Object.keys(this.active).length == 0)
            this.idleStart = event.time;
          break;
      }
    }
  },

  /**
   * Map of selected requests.
   * @type {Object.<number, boolean>}
   */
  selected: {},

  /**
   * Map of requests which has started, but are not completed yet, from
   * a request id to the chart element index.
   * @type {Object.<number, number>}}
   */
  active: {},

  /**
   * List of chart elements, calculated from the model.
   * @type {Array.<Object>}
   */
  chart: [],

  /**
   * List of rows in the chart, with the last endTime value on it.
   * @type {Array.<Object>}
   */
  rows: [],

  /**
   * Scale of the chart.
   * @type {number}
   */
  scale: 1,

  /**
   * Time of the first created request.
   * @type {Date}
   */
  timeStart: null,

  /**
   * Time of the last idling started.
   * @type {Date}
   */
  idleStart: null,

  /**
   * Total idling time since chart generation started. Used to avoid
   * generating gaps in the chart when there is no activity. In milliseconds.
   * @type {number}
   */
  idleTotal: 0,

  /**
   * List of requests information maps.
   * @type {Array.<Object>}
   */
  model: []
});

/*
 * Updates the mounted file system list.
 * @param {Array.<Object>} fileSystems Array containing provided file system
 *     information.
 */
function updateFileSystems(fileSystems) {
  var fileSystemsNode = document.querySelector('#file-systems');
  fileSystemsNode.model = fileSystems;
}

/**
 * Called when a request is created.
 * @param {Object} event Event.
 */
function onRequestEvent(event) {
  event.time = new Date(event.time);  // Convert to a real Date object.
  var requestTimelineNode = document.querySelector('#request-timeline');
  requestTimelineNode.model.push(event);
}

document.addEventListener('DOMContentLoaded', function() {
  var context = document.getCSSCanvasContext('2d', 'dashedPattern', 4, 4);
  context.beginPath();
  context.strokeStyle = '#ffffff';
  context.moveTo(0, 0);
  context.lineTo(4, 4);
  context.stroke();

  chrome.send('updateFileSystems');

  // Refresh periodically.
  setInterval(function() {
    chrome.send('updateFileSystems');
  }, 1000);
});
