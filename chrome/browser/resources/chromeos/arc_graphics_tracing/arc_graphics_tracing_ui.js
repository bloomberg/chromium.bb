// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview ARC Graphics Tracing UI.
 */

// Namespace of SVG elements
var svgNS = 'http://www.w3.org/2000/svg';

// Background color for the band with events.
var bandColor = '#d3d3d3';

// Color that should never appear on UI.
var unusedColor = '#ff0000';

/**
 * Keep in sync with ArcTracingGraphicsModel::BufferEventType
 * See chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h.
 * Describes how events should be rendered. |color| specifies color of the
 * event, |name| is used in tooltips.
 */
var eventAttributes = {
  // kBufferQueueDequeueStart
  100: {color: '#99cc00', name: 'app requests buffer'},
  // kBufferQueueDequeueDone
  101: {color: '#669999', name: 'app fills buffer'},
  // kBufferQueueQueueStart
  102: {color: '#cccc00', name: 'app queues buffer'},
  // kBufferQueueQueueDone
  103: {color: unusedColor, name: 'buffer is queued'},
  // kBufferQueueAcquire
  104: {color: '#66ffcc', name: 'use buffer'},
  // kBufferQueueReleased
  105: {color: unusedColor, name: 'buffer released'},

  // kExoSurfaceAttach.
  200: {color: '#99ccff', name: 'surface attach'},
  // kExoProduceResource
  201: {color: '#cc66ff', name: 'produce resource'},
  // kExoBound
  202: {color: '#66ffff', name: 'buffer bound'},
  // kExoPendingQuery
  203: {color: '#00ff99', name: 'pending query'},
  // kExoReleased
  204: {color: unusedColor, name: 'released'},

  // kChromeBarrierOrder.
  300: {color: '#ff9933', name: 'barrier order'},
  // kChromeBarrierFlush
  301: {color: unusedColor, name: 'barrier flush'},

  // kVsync
  400: {color: '#ff3300', name: 'vsync'},
  // kSurfaceFlingerInvalidationStart
  401: {color: '#ff9933', name: 'invalidation start'},
  // kSurfaceFlingerInvalidationDone
  402: {color: unusedColor, name: 'invalidation done'},
  // kSurfaceFlingerCompositionStart
  403: {color: '#3399ff', name: 'composition start'},
  // kSurfaceFlingerCompositionDone
  404: {color: unusedColor, name: 'composition done'},

  // kChromeOSDraw
  500: {color: '#3399ff', name: 'draw'},
  // kChromeOSSwap
  501: {color: '#cc9900', name: 'swap'},
  // kChromeOSWaitForAck
  502: {color: '#ccffff', name: 'wait for ack'},
  // kChromeOSPresentationDone
  503: {color: '#ffbf00', name: 'presentation done'},
  // kChromeOSSwapDone
  504: {color: '#65f441', name: 'swap done'},
};

/**
 * Defines the map of events that can be treated as the end of event sequence.
 * Time after such events is considered as idle time until the next event
 * starts. Key of |endSequenceEvents| is event type as defined in
 * ArcTracingGraphicsModel::BufferEventType and value is the list of event
 * types that should follow after the tested event to consider it as end of
 * sequence. Empty list means that tested event is certainly end of the
 * sequence.
 */
var endSequenceEvents = {
  // kBufferQueueQueueDone
  103: [],
  // kBufferQueueReleased
  105: [],
  // kExoReleased
  204: [],
  // kChromeBarrierFlush
  301: [],
  // kSurfaceFlingerInvalidationDone
  402: [],
  // kSurfaceFlingerCompositionDone
  404: [],
  // kChromeOSPresentationDone. Chrome does not define exactly which event
  // is the last. Different
  // pipelines may produce different sequences. Both event type may indicate
  // the end of the
  /// sequence.
  503: [500 /* kChromeOSDraw */],
  // kChromeOSSwapDone
  504: [500 /* kChromeOSDraw */],
};

/**
 * Converts timestamp into pixel offset. 1 pixel corresponds 100 microseconds.
 *
 * @param {number} timestamp in microseconds.
 */
function timestampToOffset(timestamp) {
  return timestamp / 100.0;
}

/**
 * Opposite conversion of |timestampToOffset|
 *
 * @param {number} offset in pixels.
 */
function offsetToTime(offset) {
  return offset * 100.0;
}

/**
 * Returns text representation of timestamp in milliseconds with one number
 * after the decimal point.
 *
 * @param {number} timestamp in microseconds.
 */
function timestempToMsText(timestamp) {
  return (timestamp / 1000.0).toFixed(1);
}

/** Factory class for SVG elements. */
class SVG {
  // Creates rectangle element in the |svg| with provided attributes.
  static addRect(svg, x, y, width, height, color) {
    var rect = document.createElementNS(svgNS, 'rect');
    rect.setAttributeNS(null, 'x', x);
    rect.setAttributeNS(null, 'y', y);
    rect.setAttributeNS(null, 'width', width);
    rect.setAttributeNS(null, 'height', height);
    rect.setAttributeNS(null, 'fill', color);
    rect.setAttributeNS(null, 'stroke', 'none');
    svg.appendChild(rect);
  }

  // Creates circle element in the |svg| with provided attributes.
  static addCircle(svg, x, y, radius, strokeWidth, color, strokeColor) {
    var circle = document.createElementNS(svgNS, 'circle');
    circle.setAttributeNS(null, 'cx', x);
    circle.setAttributeNS(null, 'cy', y);
    circle.setAttributeNS(null, 'r', radius);
    circle.setAttributeNS(null, 'fill', color);
    circle.setAttributeNS(null, 'stroke', strokeColor);
    circle.setAttributeNS(null, 'stroke-width', strokeWidth);
    svg.appendChild(circle);
  }

  // Creates text element in the |svg| with provided attributes.
  static addText(svg, x, y, fontSize, textContent) {
    var text = document.createElementNS(svgNS, 'text');
    text.setAttributeNS(null, 'x', x);
    text.setAttributeNS(null, 'y', y);
    text.setAttributeNS(null, 'fill', 'black');
    text.setAttributeNS(null, 'font-size', fontSize);
    text.appendChild(document.createTextNode(textContent));
    svg.appendChild(text);
  }
}

/**
 * Represents title for events bands that can collapse/expand controlled
 * content.
 */
class EventBandTitle {
  constructor(title, opt_iconContent) {
    this.div = document.createElement('div');
    this.div.classList.add('arc-events-band-title');
    if (opt_iconContent) {
      var icon = document.createElement('img');
      icon.src = 'data:image/png;base64,' + opt_iconContent;
      this.div.appendChild(icon);
    }
    var span = document.createElement('span');
    span.appendChild(document.createTextNode(title));
    this.div.appendChild(span);
    this.controlledItems = [];
    this.div.onclick = this.onClick_.bind(this);
    var parent = $('arc-event-bands');
    parent.appendChild(this.div);
  }

  /**
   * Adds extra HTML element under the control. This element will be
   * automatically expanded/collapsed together with this title.
   *
   * @param {HTMLElement} item svg element to control.
   */
  addContolledItems(item) {
    this.controlledItems.push(item);
  }

  onClick_() {
    this.div.classList.toggle('hidden');
    for (var i = 0; i < this.controlledItems.length; ++i) {
      this.controlledItems[i].classList.toggle('hidden');
    }
  }
}

/** Represents one band with events. */
class EventBand {
  /**
   * Creates band with events.
   *
   * @param {EventBandTitle} title controls visibility of this band.
   * @param {string} className class name of the svg element that represents
   *     this band. 'arc-events-top-band' is used for top-level events and
   *     'arc-events-inner-band' is used for per-buffer events.
   * @param {number} duration of band in microseconds.
   * @param {height} height of elements in this band.
   */
  constructor(title, className, duration, height) {
    this.duration = duration;
    this.width = timestampToOffset(duration);
    this.height = height;
    this.svg = document.createElementNS(svgNS, 'svg');
    this.svg.setAttributeNS(
        'http://www.w3.org/2000/xmlns/', 'xmlns:xlink',
        'http://www.w3.org/1999/xlink');
    this.svg.setAttribute('width', this.width + 'px');
    this.svg.setAttribute('height', this.height + 'px');
    this.svg.classList.add(className);

    this.setTooltip_();
    title.addContolledItems(this.svg);
    var parent = $('arc-event-bands');
    parent.appendChild(this.svg);
  }

  /**
   * Assigns events for this band. Events with type between |eventTypeMin|
   * and |eventTypeMax| are only displayed on the band.
   *
   * @param {Object[]} events non-filtered list of all events. Each has
   *     array where first element is type and second is timestamp.
   * @param {number} eventTypeMin minimum inclusive type of the event to be
   *     displayed on this band.
   * @param {number} eventTypeMax maximum inclusive type of the event to be
   *     displayed on this band.
   */
  setEvents(events, eventTypeMin, eventTypeMax) {
    this.events = events;
    this.eventTypeMin = eventTypeMin;
    this.eventTypeMax = eventTypeMax;
    this.drawBand_();
  }

  /** Enumerates events and creates visual representation */
  drawBand_() {
    var currentColor = bandColor;
    var x = 0;
    for (var i = 0; i < this.events.length; ++i) {
      var event = this.events[i];
      if (event[0] < this.eventTypeMin || event[0] > this.eventTypeMax) {
        continue;
      }
      var nextX = timestampToOffset(event[1]);
      SVG.addRect(this.svg, x, 0, nextX - x, this.height, currentColor);
      if (this.isEndOfSequence_(i)) {
        currentColor = bandColor;
      } else {
        currentColor = eventAttributes[event[0]].color;
      }
      x = nextX;
    }
    SVG.addRect(this.svg, x, 0, this.width - x, this.height, currentColor);
  }

  /** Updates tooltip and shows it for this band. */
  showToolTip_(event) {
    this.updateToolTip_(event);
    this.tooltip.classList.add('active');
  }

  /** Hides the tooltip. */
  hideToolTip_() {
    this.tooltip.classList.remove('active');
  }

  /**
   * Helper that finds next or previous event. Events that pass filter are
   * only processed.
   *
   * @param {number} index starting index for the search, not inclusive.
   * @param {direction} direction to search, 1 means to find the next event
   *     and -1 means the previous event.
   * @returns {number} index of the next or previous event or -1 in case
   *     not found.
   */
  getNextEvent_(index, direction) {
    while (true) {
      index += direction;
      if (index < 0 || index >= this.events.length) {
        return -1;
      }
      if (this.events[index][0] >= this.eventTypeMin &&
          this.events[index][0] <= this.eventTypeMax) {
        return index;
      }
    }
  }

  /**
   * Helper that returns render attributes for the event.
   *
   * @param {number} index element index in |this.events|.
   */
  getEventAttributes_(index) {
    return eventAttributes[this.events[index][0]];
  }

  /**
   * Returns true if the tested event denotes end of event sequence.
   *
   * @param {number} index element index in |this.events|.
   */
  isEndOfSequence_(index) {
    var nextEventTypes = endSequenceEvents[this.events[index][0]];
    if (!nextEventTypes) {
      return false;
    }
    if (nextEventTypes.length == 0) {
      return true;
    }
    var nextIndex = this.getNextEvent_(index, 1 /* direction */);
    if (nextIndex < 0) {
      // No more events after and it is listed as possible end of sequence.
      return true;
    }
    return nextEventTypes.includes(this.events[nextIndex][0]);
  }

  /**
   * Updates tool tip based on event under the current cursor.
   *
   * @param {Object} event mouse event.
   */
  updateToolTip_(event) {
    // Clear previous content.
    this.tooltip.textContent = '';

    // Tooltip constants for rendering.
    var horizontalGap = 10;
    var eventIconOffset = 70;
    var eventIconRadius = 4;
    var eventNameOffset = 78;
    var verticalGap = 5;
    var lineHeight = 16;
    var fontSize = 12;

    var yOffset = verticalGap + lineHeight;
    var eventTimestamp = offsetToTime(event.offsetX);
    SVG.addText(
        this.tooltip, horizontalGap, yOffset, fontSize,
        timestempToMsText(eventTimestamp) + ' ms');
    yOffset += lineHeight;

    // Find the event under the cursor. |index| points to the current event
    // and |nextIndex| points to the next event.
    var nextIndex = this.getNextEvent_(-1 /* index */, 1 /* direction */);
    while (nextIndex >= 0) {
      if (this.events[nextIndex][1] > eventTimestamp) {
        break;
      }
      nextIndex = this.getNextEvent_(nextIndex, 1 /* direction */);
    }
    var index = this.getNextEvent_(nextIndex, -1 /* direction */);

    // In case cursor points to idle event, show its interval. Otherwise
    // show the sequence of non-idle events.
    if (index < 0 || this.isEndOfSequence_(index)) {
      var startIdle = index < 0 ? 0 : this.events[index][1];
      var endIdle = index < 0 ? this.duration : this.events[nextIndex][1];
      SVG.addText(
          this.tooltip, horizontalGap, yOffset, 12,
          'Idle ' + timestempToMsText(startIdle) + '...' +
              timestempToMsText(endIdle) + ' ms.');
      yOffset += lineHeight;
    } else {
      // Find the start of the non-idle sequence.
      while (true) {
        var prevIndex = this.getNextEvent_(index, -1 /* direction */);
        if (prevIndex < 0 || this.isEndOfSequence_(prevIndex)) {
          break;
        }
        index = prevIndex;
      }

      var sequenceTimestamp = this.events[index][1];
      var lastTimestamp = sequenceTimestamp;
      var firstEvent = true;
      // Scan for the entries to show.
      var entriesToShow = [];
      while (index >= 0) {
        var attributes = this.getEventAttributes_(index);
        var eventTimestamp = this.events[index][1];
        var entryToShow = {};
        if (firstEvent) {
          // Show the global timestamp.
          entryToShow.prefix = timestempToMsText(sequenceTimestamp) + ' ms';
          firstEvent = false;
        } else {
          // Show the offset relative to the start of sequence of events.
          entryToShow.prefix = '+' +
              timestempToMsText(eventTimestamp - sequenceTimestamp) + ' ms';
        }
        entryToShow.color = attributes.color;
        entryToShow.text = attributes.name;
        if (entriesToShow.length > 0) {
          entriesToShow[entriesToShow.length - 1].text +=
              ' [' + timestempToMsText(eventTimestamp - lastTimestamp) + ' ms]';
        }
        entriesToShow.push(entryToShow);
        if (this.isEndOfSequence_(index)) {
          break;
        }
        lastTimestamp = eventTimestamp;
        index = this.getNextEvent_(index, 1 /* direction */);
      }

      // Last element is end of sequence, use bandColor for the icon.
      if (entriesToShow.length > 0) {
        entriesToShow[entriesToShow.length - 1].color = bandColor;
      }
      for (var i = 0; i < entriesToShow.length; ++i) {
        var entryToShow = entriesToShow[i];
        SVG.addText(
            this.tooltip, horizontalGap, yOffset, fontSize, entryToShow.prefix);
        SVG.addCircle(
            this.tooltip, eventIconOffset, yOffset - eventIconRadius,
            eventIconRadius, 1, entryToShow.color, 'black');
        SVG.addText(
            this.tooltip, eventNameOffset, yOffset, fontSize, entryToShow.text);
        yOffset += lineHeight;
      }
    }
    yOffset += verticalGap;

    this.tooltip.style.left = event.clientX + 'px';
    this.tooltip.style.top = event.clientY + 'px';
    this.tooltip.style.height = yOffset + 'px';
  }

  /** Initializes tooltip support by observing mouse events */
  setTooltip_() {
    this.tooltip = getSVGElement('arc-event-band-tooltip');
    this.svg.onmouseover = this.showToolTip_.bind(this);
    this.svg.onmouseout = this.hideToolTip_.bind(this);
    this.svg.onmousemove = this.updateToolTip_.bind(this);
  }
}

/**
 * Creates visual representation of graphic buffers event model.
 *
 * @param {Object} model object produced by |ArcTracingGraphicsModel|.
 */
function setGraphicBuffersModel(model) {
  // Clear previous content.
  $('arc-event-bands').textContent = '';

  var chromeTopBandTitle = new EventBandTitle('Chrome');
  var chromeTopBand = new EventBand(
      chromeTopBandTitle, 'arc-events-top-band', model.duration, 20);
  chromeTopBand.setEvents(model.chrome, 500, 504);

  var androidTopBandTitle = new EventBandTitle('Android');
  var androidTopBand = new EventBand(
      androidTopBandTitle, 'arc-events-top-band', model.duration, 20);
  androidTopBand.setEvents(model.android, 401, 504);

  for (i = 0; i < model.views.length; i++) {
    var view = model.views[i];
    var activityTitleText;
    var icon;
    if (model.tasks && view.task_id in model.tasks) {
      activityTitleText =
          model.tasks[view.task_id].title + ' - ' + view.activity;
      icon = model.tasks[view.task_id].icon;
    } else {
      activityTitleText = 'Task #' + view.task_id + ' - ' + view.activity;
    }
    var activityTitle = new EventBandTitle(activityTitleText, icon);
    for (j = 0; j < view.buffers.length; j++) {
      var androidBand = new EventBand(
          activityTitle, 'arc-events-inner-band', model.duration, 14);
      androidBand.setEvents(view.buffers[j], 100, 105);
      var exoBand = new EventBand(
          activityTitle, 'arc-events-inner-band', model.duration, 14);
      exoBand.setEvents(view.buffers[j], 200, 204);
      var chromeBand = new EventBand(
          activityTitle, 'arc-events-inner-band-last-buffer', model.duration,
          14);
      chromeBand.setEvents(view.buffers[j], 300, 301);
    }
  }
}
