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
  // kBufferFillJank
  106: {color: '#ff0000', name: 'buffer filling jank', width: 1.0},

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
  // kExoJank
  205: {color: '#ff0000', name: 'surface attach jank', width: 1.0},

  // kChromeBarrierOrder.
  300: {color: '#ff9933', name: 'barrier order'},
  // kChromeBarrierFlush
  301: {color: unusedColor, name: 'barrier flush'},

  // kVsync
  400: {color: '#ff3300', name: 'vsync', width: 0.5},
  // kSurfaceFlingerInvalidationStart
  401: {color: '#ff9933', name: 'invalidation start'},
  // kSurfaceFlingerInvalidationDone
  402: {color: unusedColor, name: 'invalidation done'},
  // kSurfaceFlingerCompositionStart
  403: {color: '#3399ff', name: 'composition start'},
  // kSurfaceFlingerCompositionDone
  404: {color: unusedColor, name: 'composition done'},
  // kSurfaceFlingerCompositionJank
  405: {color: '#ff0000', name: 'Android composition jank', width: 1.0},

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
  // kChromeOSJank
  505: {color: '#ff0000', name: 'Chrome composition jank', width: 1.0},
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

  // Creates line element in the |svg| with provided attributes.
  static addLine(svg, x1, y1, x2, y2, color, width) {
    var line = document.createElementNS(svgNS, 'line');
    line.setAttributeNS(null, 'x1', x1);
    line.setAttributeNS(null, 'y1', y1);
    line.setAttributeNS(null, 'x2', x2);
    line.setAttributeNS(null, 'y2', y2);
    line.setAttributeNS(null, 'stroke', color);
    line.setAttributeNS(null, 'stroke-width', width);
    svg.appendChild(line);
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

/** Represents container for event bands. */
class EventBands {
  /**
   * Creates container for the event bands.
   *
   * @param {EventBandTitle} title controls visibility of this band.
   * @param {string} className class name of the svg element that represents
   *     this band. 'arc-events-top-band' is used for top-level events and
   *     'arc-events-inner-band' is used for per-buffer events.
   * @param {number} duration of bands in microseconds.
   */
  constructor(title, className, duration) {
    // Keep information about bands and their bounds.
    this.bands = [];
    this.globalEvents = [];
    this.duration = duration;
    this.width = timestampToOffset(duration);
    this.height = 0;
    // Offset of the next band of events.
    this.nextYOffset = 0;
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
   * This adds new band of events. Height of svg container is automatically
   * adjusted to fit the new content.
   *
   * @param {Events} eventBand event band to add.
   * @param {number} height of the band.
   * @param {number} padding to separate from the next band.
   */
  addBand(eventBand, height, padding) {
    var currentColor = bandColor;
    var x = 0;
    var eventIndex = -1;
    while (true) {
      eventIndex = eventBand.getNextEvent(eventIndex, 1 /* direction */);
      if (eventIndex < 0) {
        break;
      }
      var event = eventBand.events[eventIndex];
      var nextX = timestampToOffset(event[1]);
      SVG.addRect(
          this.svg, x, this.nextYOffset, nextX - x, height, currentColor);
      if (eventBand.isEndOfSequence(eventIndex)) {
        currentColor = bandColor;
      } else {
        currentColor = eventAttributes[event[0]].color;
      }
      x = nextX;
    }
    SVG.addRect(
        this.svg, x, this.nextYOffset, this.width - x, height, currentColor);

    this.bands.push({
      band: eventBand,
      top: this.nextYOffset,
      bottom: this.nextYOffset + height
    });

    this.nextYOffset += height;
    this.height = this.nextYOffset;
    this.svg.setAttribute('height', this.height + 'px');
    this.nextYOffset += padding;
  }

  /**
   * This adds events as a global events that do not belong to any band.
   *
   * @param {Events} events to add.
   */
  addGlobal(events) {
    var eventIndex = -1;
    while (true) {
      eventIndex = events.getNextEvent(eventIndex, 1 /* direction */);
      if (eventIndex < 0) {
        break;
      }
      var event = events.events[eventIndex];
      var attributes = events.getEventAttributes(eventIndex);
      var x = timestampToOffset(event[1]);
      SVG.addLine(
          this.svg, x, 0, x, this.height, attributes.color, attributes.width);
    }
    this.globalEvents.push(events);
  }

  /** Initializes tooltip support by observing mouse events */
  setTooltip_() {
    this.tooltip = getSVGElement('arc-event-band-tooltip');
    this.svg.onmouseover = this.showToolTip_.bind(this);
    this.svg.onmouseout = this.hideToolTip_.bind(this);
    this.svg.onmousemove = this.updateToolTip_.bind(this);
  }

  /** Updates tooltip and shows it for this band. */
  showToolTip_(event) {
    this.updateToolTip_(event);
  }

  /** Hides the tooltip. */
  hideToolTip_() {
    this.tooltip.classList.remove('active');
  }

  /**
   * Finds the global event that is closest to the |timestamp| and not farther
   * than |distance|.
   *
   * @param {number} timestamp to search.
   * @param {number} distance to search.
   */
  findGlobalEvent_(timestamp, distance) {
    var bestDistance = distance;
    var bestEvent = null;
    for (var i = 0; i < this.globalEvents.length; ++i) {
      var globalEvents = this.globalEvents[i];
      var closestIndex = this.globalEvents[i].getClosest(timestamp);
      if (closestIndex >= 0) {
        var testEvent = this.globalEvents[i].events[closestIndex];
        var testDistance = Math.abs(testEvent[1] - timestamp);
        if (testDistance < bestDistance) {
          bestDistance = testDistance;
          bestEvent = testEvent;
        }
      }
    }
    return bestEvent;
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

    // Find band for this mouse event.
    var eventBand = undefined;

    for (var i = 0; i < this.bands.length; ++i) {
      if (this.bands[i].top <= event.offsetY &&
          this.bands[i].bottom > event.offsetY) {
        eventBand = this.bands[i].band;
        break;
      }
    }

    if (!eventBand) {
      this.tooltip.classList.remove('active');
      return;
    }

    var yOffset = verticalGap + lineHeight;
    var eventTimestamp = offsetToTime(event.offsetX);
    SVG.addText(
        this.tooltip, horizontalGap, yOffset, fontSize,
        timestempToMsText(eventTimestamp) + ' ms');
    yOffset += lineHeight;

    // Find the event under the cursor. |index| points to the current event
    // and |nextIndex| points to the next event.
    var nextIndex = eventBand.getNextEvent(-1 /* index */, 1 /* direction */);
    while (nextIndex >= 0) {
      if (eventBand.events[nextIndex][1] > eventTimestamp) {
        break;
      }
      nextIndex = eventBand.getNextEvent(nextIndex, 1 /* direction */);
    }
    var index = eventBand.getNextEvent(nextIndex, -1 /* direction */);

    // Try to find closest global event in the range -200..200 mcs from
    // |eventTimestamp|.
    var globalEvent = this.findGlobalEvent_(eventTimestamp, 200 /* distance */);
    if (globalEvent) {
      // Show the global event info.
      var attributes = eventAttributes[global_event[0]];
      SVG.addText(
          this.tooltip, horizontalGap, yOffset, 12,
          attributes.name + ' ' + timestempToMsText(globalEvent[1]) + ' ms.');
    } else if (index < 0 || eventBand.isEndOfSequence(index)) {
      // In case cursor points to idle event, show its interval.
      var startIdle = index < 0 ? 0 : eventBand.events[index][1];
      var endIdle = index < 0 ? this.duration : eventBand.events[nextIndex][1];
      SVG.addText(
          this.tooltip, horizontalGap, yOffset, 12,
          'Idle ' + timestempToMsText(startIdle) + '...' +
              timestempToMsText(endIdle) + ' ms.');
      yOffset += lineHeight;
    } else {
      // Show the sequence of non-idle events.
      // Find the start of the non-idle sequence.
      while (true) {
        var prevIndex = eventBand.getNextEvent(index, -1 /* direction */);
        if (prevIndex < 0 || eventBand.isEndOfSequence(prevIndex)) {
          break;
        }
        index = prevIndex;
      }

      var sequenceTimestamp = eventBand.events[index][1];
      var lastTimestamp = sequenceTimestamp;
      var firstEvent = true;
      // Scan for the entries to show.
      var entriesToShow = [];
      while (index >= 0) {
        var attributes = eventBand.getEventAttributes(index);
        var eventTimestamp = eventBand.events[index][1];
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
        if (eventBand.isEndOfSequence(index)) {
          break;
        }
        lastTimestamp = eventTimestamp;
        index = eventBand.getNextEvent(index, 1 /* direction */);
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
    this.tooltip.classList.add('active');
  }
}

/** Represents one band with events. */
class Events {
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
  constructor(events, eventTypeMin, eventTypeMax) {
    this.events = events;
    this.eventTypeMin = eventTypeMin;
    this.eventTypeMax = eventTypeMax;
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
  getNextEvent(index, direction) {
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
  getEventAttributes(index) {
    return eventAttributes[this.events[index][0]];
  }

  /**
   * Returns true if the tested event denotes end of event sequence.
   *
   * @param {number} index element index in |this.events|.
   */
  isEndOfSequence(index) {
    var nextEventTypes = endSequenceEvents[this.events[index][0]];
    if (!nextEventTypes) {
      return false;
    }
    if (nextEventTypes.length == 0) {
      return true;
    }
    var nextIndex = this.getNextEvent(index, 1 /* direction */);
    if (nextIndex < 0) {
      // No more events after and it is listed as possible end of sequence.
      return true;
    }
    return nextEventTypes.includes(this.events[nextIndex][0]);
  }

  /**
   * Returns the index of closest event to the requested |timestamp|.
   *
   * @param {number} timestamp to search.
   */
  getClosest(timestamp) {
    if (this.events.length == 0) {
      return -1;
    }
    if (this.events[0][1] >= timestamp) {
      return this.getNextEvent(-1 /* index */, 1 /* direction */);
    }
    if (this.events[this.events.length - 1][1] <= timestamp) {
      return this.getNextEvent(
          this.events.length /* index */, -1 /* direction */);
    }
    // At this moment |firstBefore| and |firstAfter| points to any event.
    var firstBefore = 0;
    var firstAfter = this.events.length - 1;
    while (firstBefore + 1 != firstAfter) {
      var candidateIndex = Math.ceil((firstBefore + firstAfter) / 2);
      if (this.events[candidateIndex][1] < timestamp) {
        firstBefore = candidateIndex;
      } else {
        firstAfter = candidateIndex;
      }
    }
    // Point |firstBefore| and |firstAfter| to the supported event types.
    firstBefore =
        this.getNextEvent(firstBefore + 1 /* index */, -1 /* direction */);
    firstAfter =
        this.getNextEvent(firstAfter - 1 /* index */, 1 /* direction */);
    if (firstBefore < 0) {
      return firstAfter;
    } else if (firstAfter < 0) {
      return firstBefore;
    } else {
      var diffBefore = timestamp - this.events[firstBefore][1];
      var diffAfter = this.events[firstAfter][1] - timestamp;
      if (diffBefore < diffAfter) {
        return firstBefore;
      } else {
        return firstAfter;
      }
    }
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

  var vsyncEvents = new Events(
      model.android.global_events, 400 /* kVsync */, 400 /* kVsync */);

  var chromeTitle = new EventBandTitle('Chrome');
  var chromeBands =
      new EventBands(chromeTitle, 'arc-events-top-band', model.duration);
  for (i = 0; i < model.chrome.buffers.length; i++) {
    chromeBands.addBand(
        new Events(model.chrome.buffers[i], 500, 599), 20 /* height */,
        4 /* padding */);
  }
  chromeBands.addGlobal(new Events(
      model.chrome.global_events, 505 /* kChromeOSJank */,
      505 /* kChromeOSJank */));

  var androidTitle = new EventBandTitle('Android');
  var androidBands =
      new EventBands(androidTitle, 'arc-events-top-band', model.duration);
  androidBands.addBand(
      new Events(model.android.buffers[0], 400, 499), 20 /* height */,
      0 /* padding */);
  // Add vsync events
  androidBands.addGlobal(vsyncEvents);
  androidBands.addGlobal(new Events(
      model.android.global_events, 405 /* kSurfaceFlingerCompositionJank */,
      405 /* kSurfaceFlingerCompositionJank */));

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
    var activityBands =
        new EventBands(activityTitle, 'arc-events-inner-band', model.duration);
    for (j = 0; j < view.buffers.length; j++) {
      var androidBand = new Events(
          activityTitle, 'arc-events-inner-band', model.duration, 14);
      // Android buffer events.
      activityBands.addBand(
          new Events(view.buffers[j], 100, 199), 14 /* height */,
          4 /* padding */);
      // exo events.
      activityBands.addBand(
          new Events(view.buffers[j], 200, 299), 14 /* height */,
          4 /* padding */);
      // Chrome buffer events.
      activityBands.addBand(
          new Events(view.buffers[j], 300, 399), 14 /* height */,
          12 /* padding */);
    }
    // Add vsync events
    activityBands.addGlobal(vsyncEvents);
    activityBands.addGlobal(new Events(
        view.global_events, 106 /* kBufferFillJank */,
        106 /* kBufferFillJank */));
  }
}
