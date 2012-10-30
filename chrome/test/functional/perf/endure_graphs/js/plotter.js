/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/**
 * @fileoverview Collection of functions and classes used to plot data in a
 *     <canvas>.  Create a Plotter() to generate a plot.
 */

/**
 * Adds commas to a given number.
 *
 * Examples:
 *  1234.56 => "1,234.56"
 *  99999 => "99,999"
 *
 * @param {string|number} number The number to format.
 * @return {string} String representation of |number| with commas for every
 *     three digits to the left of a decimal point.
 */
function addCommas(number) {
  number += '';  // Convert number to string if not already a string.
  var numberParts = number.split('.');
  var integralPart = numberParts[0];
  var fractionalPart = numberParts.length > 1 ? '.' + numberParts[1] : '';
  var reThreeDigits = /(\d+)(\d{3})/;
  while (reThreeDigits.test(integralPart))
    integralPart = integralPart.replace(reThreeDigits, '$1' + ',' + '$2');
  return integralPart + fractionalPart;
}

/**
 * Vertical marker to highlight data points that are being hovered over by the
 * mouse.
 *
 * @param {string} color The color to make the marker, e.g., 'rgb(100,80,240)'.
 * @return {Element} A div Element object representing the vertical marker.
 */
function VerticalMarker(color) {
  var m = document.createElement('div');
  m.style.backgroundColor = color;
  m.style.opacity = '0.3';
  m.style.position = 'absolute';
  m.style.left = '-2px';
  m.style.top = '-2px';
  m.style.width = '0px';
  m.style.height = '0px';
  return m;
}

/**
 * Class representing a horizontal marker at the indicated mouse location.
 * @constructor
 *
 * @param {Element} canvasElement The canvas bounds.
 * @param {Number} yValue The data value corresponding to the vertical click
 *     location.
 * @param {Number} yOtherValue If the plot is overlaying two coordinate systems,
 *     this is the data value corresponding to the vertical click location in
 *     the second coordinate system.  Can be null.
 */
function HorizontalMarker(canvasElement, yValue, yOtherValue) {
  var m = document.createElement('div');
  m.style.backgroundColor = HorizontalMarker.COLOR;
  m.style.opacity = '0.3';
  m.style.position = 'absolute';
  m.style.width = canvasElement.offsetWidth + 'px';
  m.style.height = HorizontalMarker.HEIGHT + 'px';

  this.markerDiv = m;
  this.value = yValue;
  this.otherValue = yOtherValue;
}

HorizontalMarker.HEIGHT = 5;
HorizontalMarker.COLOR = 'rgb(0,100,100)';

/**
 * Locates this element at a specified position.
 *
 * @param {Element} canvasElement The canvas element at which this element is
 *     to be placed.
 * @param {number} y Y position relative to the canvas element.
 */
HorizontalMarker.prototype.locateAt = function(canvasElement, y) {
  var div = this.markerDiv;
  div.style.left = domUtils.pageXY(canvasElement).x -
                   domUtils.pageXY(div.offsetParent) + 'px';
  div.style.top = (y + domUtils.pageXY(canvasElement).y
                   - domUtils.pageXY(div.offsetParent).y
                   - (HorizontalMarker.HEIGHT / 2)) + 'px';
};

/**
 * Removes the horizontal marker from the graph.
 */
HorizontalMarker.prototype.remove = function() {
  this.markerDiv.parentNode.removeChild(this.markerDiv);
};

/**
 * An information indicator hovering around the mouse cursor on the graph.
 * This class is used to show a legend near the mouse cursor.
 *
 * A set of legends under the graph is managed separately in
 * {@code Plotter.createLegendsSummaryElement_}.
 *
 * @constructor
 */
function HoveringInfo() {
  this.containerDiv_ = document.createElement('div');
  this.containerDiv_.style.display = 'none';
  this.containerDiv_.style.position = 'absolute';
  this.containerDiv_.style.border = '1px solid #000';
  this.containerDiv_.style.padding = '0.12em';
  this.containerDiv_.style.backgroundColor = '#ddd';
  this.colorIndicator_ = document.createElement('div');
  this.colorIndicator_.style.display = 'inline-block';
  this.colorIndicator_.style.width = '1em';
  this.colorIndicator_.style.height = '1em';
  this.colorIndicator_.style.verticalAlign = 'text-bottom';
  this.colorIndicator_.style.margin = '0 0.24em 0 0';
  this.colorIndicator_.style.border = '1px solid #000';
  this.legendText_ = document.createElement('span');
  this.itemValueText_ = document.createElement('span');

  this.containerDiv_.appendChild(this.colorIndicator_);
  this.containerDiv_.appendChild(this.legendText_);
  var div = document.createElement('div');
  div.appendChild(this.itemValueText_);
  this.containerDiv_.appendChild(div);
}

/**
 * Returns the container element;
 *
 * @return {Element} The container element.
 */
HoveringInfo.prototype.getElement = function() {
  return this.containerDiv_;
};

/**
 * Shows or hides the element.
 *
 * @param {boolean} show Shows the element if true, or hides it.
 */
HoveringInfo.prototype.show = function(show) {
  this.containerDiv_.style.display = show ? 'block' : 'none';
};

/**
 * Returns the position of the container element in the page coordinate.
 *
 * @return {Object} A point object which has {@code x} and {@code y} fields.
 */
HoveringInfo.prototype.pageXY = function() {
  return domUtils.pageXY(this.containerDiv_);
};

/**
 * Locates the element at the specified position.
 *
 * @param {number} x X position in the page coordinate.
 * @param {number} y Y position in the page coordinate.
 */
HoveringInfo.prototype.locateAtPageXY = function(x, y) {
  var parentXY = domUtils.pageXY(this.containerDiv_.offsetParent);
  this.containerDiv_.style.left = x - parentXY.x + 'px';
  this.containerDiv_.style.top = y - parentXY.y + 'px';
};

/**
 * Returns the legend text.
 *
 * @return {?string} The legend text.
 */
HoveringInfo.prototype.getLegendText = function() {
  return this.legendText_.textContent;
};

/**
 * Changes the legend text.
 *
 * @param {string} text The new text to be set.
 */
HoveringInfo.prototype.setLegendText = function(text) {
  this.legendText_.textContent = text;
};

/**
 * Changes the item value.
 *
 * @param {number} value The new value to be shown.
 */
HoveringInfo.prototype.setItemValue = function(value) {
  this.itemValueText_.textContent = 'Item value = ' + addCommas(value);
};

/**
 * Changes the color of the color indicator.
 *
 * @param {string} color The new color to be set.
 */
HoveringInfo.prototype.setColorIndicator = function(color) {
  this.colorIndicator_.style.backgroundColor = color;
};

/**
 * Main class that does the actual plotting.
 *
 * Draws a chart using a canvas element. Takes an array of lines to draw.
 * @constructor
 *
 * @param {Array} plotData list of arrays that represent individual lines. The
 *     line itself is an Array of points.
 * @param {Array} dataDescriptions list of data descriptions for each line in
 *     |plotData|.
 * @param {string} eventName The string name of an event to overlay on the
 *     graph.  Should be 'null' if there are no events to overlay.
 * @param {Object} eventInfo If |eventName| is specified, an array of event
 *     points to overlay on the graph.  Each event point in the array is itself
 *     a 2-element array, where the first element is the x-axis value at which
 *     the event occurred during the test, and the second element is a
 *     dictionary of kay/value pairs representing metadata associated with the
 *     event.
 * @param {string} unitsX The x-axis units of the data being plotted.
 * @param {string} unitsY The y-axis units of the data being plotted.
 * @param {string} unitsYOther If another graph (with different y-axis units) is
 *     being overlayed over the first graph, this represents the units of the
 *     other graph.  Otherwise, this should be 'null'.
 * @param {?number} graphsOtherStartIndex Specifies the starting index of
 *     the second set of lines.  {@code plotData} in the range of
 *     [0, {@code graphsOtherStartIndex}) are treated as the first set of lines,
 *     and ones in the range of
 *     [{@code graphsOtherStartIndex}, {@code plotData.length}) are as
 *     the second set.  0, {@code plotData.length} and {@code null} mean
 *     no second set, i.e. all the data in {@code plotData} represent the single
 *     set of lines.
 * @param {Element} resultNode A DOM Element object representing the DOM node to
 *     which the plot should be attached.
 * @param {boolean} is_lookout Whether or not the graph should be drawn
 *     in 'lookout' mode, which is a summarized view that is made for overview
 *     pages when the graph is drawn in a more confined space.
 * @param {boolean} stackedGraph Whether or not the first set of lines is
 *     a stacked graph.
 * @param {boolean} stackedGraphOther Whether or not the second set of lines is
 *     a stacked graph.
 *
 * Example of the |plotData|:
 *  [
 *    [line 1 data],
 *    [line 2 data]
 *  ].
 *  Line data looks like  [[point one], [point two]].
 *  And individual points are [x value, y value]
 */
function Plotter(plotData, dataDescriptions, eventName, eventInfo, unitsX,
                 unitsY, unitsYOther, graphsOtherStartIndex, resultNode,
                 is_lookout, stackedGraph, stackedGraphOther) {
  this.plotData_ = plotData;
  this.dataDescriptions_ = dataDescriptions;
  this.eventName_ = eventName;
  this.eventInfo_ = eventInfo;
  this.unitsX_ = unitsX;
  this.unitsY_ = unitsY;
  this.unitsYOther_ = unitsYOther;
  this.graphsOtherStartIndex_ =
      (0 < graphsOtherStartIndex && graphsOtherStartIndex < plotData.length) ?
      graphsOtherStartIndex : null;
  this.resultNode_ = resultNode;
  this.is_lookout_ = is_lookout;
  this.stackedGraph_ = stackedGraph;
  this.stackedGraphOther_ = stackedGraphOther;

  this.dataColors_ = [];

  this.coordinates = null;
  this.coordinatesOther = null;
  if (this.unitsYOther_ && this.graphsOtherStartIndex_) {
    // Need two different coordinate systems to overlay on the same graph.
    this.coordinates = new Coordinates(
        this.plotData_.slice(0, this.graphsOtherStartIndex_));
    this.coordinatesOther = new Coordinates(
        this.plotData_.slice(this.graphsOtherStartIndex_));
  } else {
    this.coordinates = new Coordinates(this.plotData_);
  }

  // A color palette that's unambigous for normal and color-deficient viewers.
  // Values are (red, green, blue) on a scale of 255.
  // Taken from http://jfly.iam.u-tokyo.ac.jp/html/manuals/pdf/color_blind.pdf.
  this.colors = [[0, 114, 178],   // Blue.
                 [230, 159, 0],   // Orange.
                 [0, 158, 115],   // Green.
                 [204, 121, 167], // Purplish pink.
                 [86, 180, 233],  // Sky blue.
                 [213, 94, 0],    // Dark orange.
                 [0, 0, 0],       // Black.
                 [240, 228, 66]   // Yellow.
                ];

  for (var i = 0, colorIndex = 0; i < this.dataDescriptions_.length; ++i)
    this.dataColors_[i] = this.makeColor(colorIndex++);
}

/**
 * Generates a string representing a color corresponding to the given index
 * in a color array.  Handles wrapping around the color array if necessary.
 *
 * @param {number} i An index into the |this.colors| array.
 * @return {string} A string representing a color in 'rgb(X,Y,Z)' format.
 */
Plotter.prototype.makeColor = function(i) {
  var index = i % this.colors.length;
  return 'rgb(' + this.colors[index][0] + ',' +
                  this.colors[index][1] + ',' +
                  this.colors[index][2] + ')';
};

/**
 * Same as function makeColor above, but also takes a transparency value
 * indicating how transparent to make the color appear.
 *
 * @param {number} i An index into the |this.colors| array.
 * @param {number} transparencyPercent Percentage transparency to make the
 *     color, e.g., 0.75.
 * @return {string} A string representing a color in 'rgb(X,Y,Z,A)' format,
 *     where A is the percentage transparency.
 */
Plotter.prototype.makeColorTransparent = function(i, transparencyPercent) {
  var index = i % this.colors.length;
  return 'rgba(' + this.colors[index][0] + ',' +
                   this.colors[index][1] + ',' +
                   this.colors[index][2] + ',' + transparencyPercent + ')';
};

/**
 * Gets the data color value associated with a specified color index.
 *
 * @param {number} i An index into the |this.colors| array.
 * @return {string} A string representing a color in 'rgb(X,Y,Z,A)' format,
 *     where A is the percentage transparency.
 */
Plotter.prototype.getDataColor = function(i) {
  if (this.dataColors_[i])
    return this.dataColors_[i];
  else
    return this.makeColor(i);
};

/**
 * Gets the fill color value associated with a specified color index.
 *
 * @param {number} i An index into the |this.colors| array.
 * @return {string} A string representing a color in 'rgba(R,G,B,A)' format,
 *     where A is the percentage transparency.
 */
Plotter.prototype.getFillColor = function(i) {
  return this.makeColorTransparent(i, 0.4);
};

/**
 * Does the actual plotting.
 */
Plotter.prototype.plot = function() {
  var self = this;

  this.canvasElement_ = this.canvas_();
  this.rulerDiv_ = this.ruler_();

  // Markers for the result point(s)/events that the mouse is currently
  // hovering over.
  this.cursorDiv_ = new VerticalMarker('rgb(100,80,240)');
  this.cursorDivOther_ = new VerticalMarker('rgb(50,50,50)');
  this.eventDiv_ = new VerticalMarker('rgb(255, 0, 0)');
  this.hoveringInfo_ = new HoveringInfo();

  this.resultNode_.appendChild(this.canvasElement_);
  this.resultNode_.appendChild(this.coordinates_());
  this.resultNode_.appendChild(this.rulerDiv_);
  this.resultNode_.appendChild(this.cursorDiv_);
  this.resultNode_.appendChild(this.cursorDivOther_);
  this.resultNode_.appendChild(this.eventDiv_);
  this.resultNode_.appendChild(this.hoveringInfo_.getElement());
  this.attachEventListeners_();

  // Now draw the canvas.
  var ctx = this.canvasElement_.getContext('2d');

  // Clear it with white: otherwise canvas will draw on top of existing data.
  ctx.clearRect(0, 0, this.canvasElement_.width, this.canvasElement_.height);

  // Draw all data lines in the reverse order so the last graph appears on
  // the backmost and the first graph appears on the frontmost.
  function draw(plotData, coordinates, colorOffset, stack) {
    for (var i = plotData.length - 1; i >= 0; --i) {
      if (stack) {
        self.plotAreaUnderLine_(ctx, self.getFillColor(colorOffset + i),
                                plotData[i], coordinates);
      }
      self.plotLine_(ctx, self.getDataColor(colorOffset + i),
                     plotData[i], coordinates);
    }
  }
  draw(this.plotData_.slice(0,
                            this.graphsOtherStartIndex_ ?
                            this.graphsOtherStartIndex_ :
                            this.plotData_.length),
       this.coordinates, 0, this.stackedGraph_);
  if (this.graphsOtherStartIndex_) {
    draw(this.plotData_.slice(this.graphsOtherStartIndex_),
         this.unitsYOther_ ? this.coordinatesOther : this.coordinates,
         this.graphsOtherStartIndex_, this.stackedGraphOther_);
  }

  // Draw events overlayed on graph if needed.
  if (this.eventName_ && this.eventInfo_)
    this.plotEvents_(ctx, 'rgb(255, 150, 150)', this.coordinates);

  this.graduation_divs_ = this.graduations_(this.coordinates, 0, false);
  if (this.unitsYOther_) {
    this.graduation_divs_ = this.graduation_divs_.concat(
        this.graduations_(this.coordinatesOther, 1, true));
  }
  for (var i = 0; i < this.graduation_divs_.length; ++i)
    this.resultNode_.appendChild(this.graduation_divs_[i]);
};

/**
 * Draws events overlayed on top of an existing graph.
 *
 * @param {Object} ctx A canvas element object for drawing.
 * @param {string} strokeStyles A string representing the drawing style.
 * @param {Object} coordinateSystem A Coordinates object representing the
 *     coordinate system of the graph.
 */
Plotter.prototype.plotEvents_ = function(ctx, strokeStyles, coordinateSystem) {
  ctx.strokeStyle = strokeStyles;
  ctx.fillStyle = strokeStyles;
  ctx.lineWidth = 1.0;

  ctx.beginPath();
  var data = this.eventInfo_;
  for (var index = 0; index < data.length; ++index) {
    var event_time = data[index][0];
    var x = coordinateSystem.xPixel(event_time);
    ctx.moveTo(x, 0);
    ctx.lineTo(x, this.canvasElement_.offsetHeight);
  }
  ctx.closePath();
  ctx.stroke();
};

/**
 * Draws a line on the graph.
 *
 * @param {Object} ctx A canvas element object for drawing.
 * @param {string} strokeStyles A string representing the drawing style.
 * @param {Array} data A list of [x, y] values representing the line to plot.
 * @param {Object} coordinateSystem A Coordinates object representing the
 *     coordinate system of the graph.
 */
Plotter.prototype.plotLine_ = function(ctx, strokeStyles, data,
                                       coordinateSystem) {
  ctx.strokeStyle = strokeStyles;
  ctx.fillStyle = strokeStyles;
  ctx.lineWidth = 2.0;

  ctx.beginPath();
  var initial = true;
  var allPoints = [];
  for (var i = 0; i < data.length; ++i) {
    var pointX = parseFloat(data[i][0]);
    var pointY = parseFloat(data[i][1]);
    var x = coordinateSystem.xPixel(pointX);
    var y = coordinateSystem.yPixel(0);
    if (isNaN(pointY)) {
      // Re-set 'initial' if we're at a gap in the data.
      initial = true;
    } else {
      y = coordinateSystem.yPixel(pointY);
      if (initial)
        initial = false;
      else
        ctx.lineTo(x, y);
    }

    ctx.moveTo(x, y);
    if (!data[i].interpolated) {
      allPoints.push([x, y]);
    }
  }
  ctx.closePath();
  ctx.stroke();

  if (!this.is_lookout_) {
    // Draw a small dot at each point.
    for (var i = 0; i < allPoints.length; ++i) {
      ctx.beginPath();
      ctx.arc(allPoints[i][0], allPoints[i][1], 3, 0, Math.PI*2, true);
      ctx.fill();
    }
  }
};

/**
 * Fills an area under the given line on the graph.
 *
 * @param {Object} ctx A canvas element object for drawing.
 * @param {string} fillStyle A string representing the drawing style.
 * @param {Array} data A list of [x, y] values representing the line to plot.
 * @param {Object} coordinateSystem A Coordinates object representing the
 *     coordinate system of the graph.
 */
Plotter.prototype.plotAreaUnderLine_ = function(ctx, fillStyle, data,
                                                coordinateSystem) {
  if (!data[0]) {
    return;  // nothing to draw
  }

  ctx.beginPath();
  var x = coordinateSystem.xPixel(parseFloat(data[0][0]) || 0);
  var y = coordinateSystem.yPixel(parseFloat(data[0][1]) || 0);
  var y0 = coordinateSystem.yPixel(coordinateSystem.yMinValue());
  ctx.moveTo(x, y0);
  for (var point, i = 0; point = data[i]; ++i) {
    var pointX = parseFloat(point[0]);
    var pointY = parseFloat(point[1]);
    if (isNaN(pointX)) { continue; }  // Skip an invalid point.
    if (isNaN(pointY)) {
      ctx.lineTo(x, y0);
      var yWasNaN = true;
    } else {
      x = coordinateSystem.xPixel(pointX);
      y = coordinateSystem.yPixel(pointY);
      if (yWasNaN) {
        ctx.lineTo(x, y0);
        yWasNaN = false;
      }
      ctx.lineTo(x, y);
    }
  }
  ctx.lineTo(x, y0);

  ctx.lineWidth = 0;
  // Clear the area with white color first.
  var COLOR_WHITE = 'rgb(255,255,255)';
  ctx.strokeStyle = COLOR_WHITE;
  ctx.fillStyle = COLOR_WHITE;
  ctx.fill();
  // Then, fill the area with the specified color.
  ctx.strokeStyle = fillStyle;
  ctx.fillStyle = fillStyle;
  ctx.fill();
};

/**
 * Attaches event listeners to DOM nodes.
 */
Plotter.prototype.attachEventListeners_ = function() {
  var self = this;
  this.canvasElement_.parentNode.addEventListener(
    'mousemove', function(evt) { self.onMouseMove_(evt); }, false);
  this.canvasElement_.parentNode.addEventListener(
    'mouseover', function(evt) { self.onMouseOver_(evt); }, false);
  this.canvasElement_.parentNode.addEventListener(
    'mouseout', function(evt) { self.onMouseOut_(evt); }, false);
  this.cursorDiv_.addEventListener(
    'click', function(evt) { self.onMouseClick_(evt); }, false);
  this.cursorDivOther_.addEventListener(
    'click', function(evt) { self.onMouseClick_(evt); }, false);
  this.eventDiv_.addEventListener(
    'click', function(evt) { self.onMouseClick_(evt); }, false);
};

/**
 * Update the horizontal line that is following where the mouse is hovering.
 *
 * @param {Object} evt A mouse event object representing a mouse move event.
 */
Plotter.prototype.updateRuler_ = function(evt) {
  var r = this.rulerDiv_;
  r.style.left = this.canvasElement_.offsetLeft + 'px';
  r.style.top = this.canvasElement_.offsetTop + 'px';
  r.style.width = this.canvasElement_.offsetWidth + 'px';
  var h = domUtils.pageXYOfEvent(evt).y -
          domUtils.pageXY(this.canvasElement_).y;
  if (h > this.canvasElement_.offsetHeight)
    h = this.canvasElement_.offsetHeight;
  r.style.height = h + 'px';
};

/**
 * Update the highlighted data point at the x value that the mouse is hovering
 * over.
 *
 * @param {Object} coordinateSystem A Coordinates object representing the
 *     coordinate system of the graph.
 * @param {number} currentIndex The index into the |this.plotData| array of the
 *     data point being hovered over, for a given line.
 * @param {Object} cursorDiv A DOM element div object representing the highlight
 *     itself.
 * @param {number} dataIndex The index into the |this.plotData| array of the
 *     line being hovered over.
 */
Plotter.prototype.updateCursor_ = function(coordinateSystem, currentIndex,
                                           cursorDiv, dataIndex) {
  var c = cursorDiv;
  c.style.top = this.canvasElement_.offsetTop + 'px';
  c.style.height = this.canvasElement_.offsetHeight + 'px';

  // Left point is half-way to the previous x value, unless it's the first
  // point, in which case it's the x value of the current point.
  var leftPoint = null;
  if (currentIndex == 0) {
    leftPoint = this.canvasElement_.offsetLeft +
        coordinateSystem.xPixel(this.plotData_[dataIndex][0][0]);
  }
  else {
    var left_x = this.canvasElement_.offsetLeft +
        coordinateSystem.xPixel(this.plotData_[dataIndex][currentIndex - 1][0]);
    var curr_x = this.canvasElement_.offsetLeft +
        coordinateSystem.xPixel(this.plotData_[dataIndex][currentIndex][0]);
    leftPoint = (left_x + curr_x) / 2;
  }
  c.style.left = leftPoint;

  // Width is half-way to the next x value minus the left point, unless it's
  // the last point, in which case it's the x value of the current point minus
  // the left point.
  if (currentIndex == this.plotData_[dataIndex].length - 1) {
    var curr_x = this.canvasElement_.offsetLeft +
        coordinateSystem.xPixel(this.plotData_[dataIndex][currentIndex][0]);
    c.style.width = curr_x - leftPoint;
  }
  else {
    var next_x = this.canvasElement_.offsetLeft +
        coordinateSystem.xPixel(this.plotData_[dataIndex][currentIndex + 1][0]);
    var curr_x = this.canvasElement_.offsetLeft +
        coordinateSystem.xPixel(this.plotData_[dataIndex][currentIndex][0]);
    c.style.width = ((next_x + curr_x) / 2) - leftPoint;
  }
};

/**
 * Update the highlighted event at the x value that the mouse is hovering over.
 *
 * @param {number} x The x-value (pixel) at which to draw the event highlight
 *     div.
 * @param {boolean} show Whether or not to show the highlight div.
 */
Plotter.prototype.updateEventDiv_ = function(x, show) {
  var c = this.eventDiv_;
  c.style.top = this.canvasElement_.offsetTop + 'px';
  c.style.height = this.canvasElement_.offsetHeight + 'px';

  if (show) {
    c.style.left = this.canvasElement_.offsetLeft + (x - 2);
    c.style.width = 8;
  } else {
    c.style.width = 0;
  }
};

/**
 * Updates the hovering information.
 *
 * @param {Event} evt An event object, which specifies the position of the mouse
 *     cursor.
 * @param {boolean} show Whether or not to show the hovering info.  Even if it's
 *     true, if the cursor position is out of the appropriate area, nothing will
 *     be shown.
 */
Plotter.prototype.updateHoveringInfo_ = function(evt, show) {
  var evtPageXY = domUtils.pageXYOfEvent(evt);
  var hoveringInfoPageXY = this.hoveringInfo_.pageXY();
  var canvasPageXY = domUtils.pageXY(this.canvasElement_);

  var coord = this.coordinates;
  // p = the mouse cursor position in value coordinates.
  var p = {'x': coord.xValue(evtPageXY.x - canvasPageXY.x),
           'y': coord.yValue(evtPageXY.y - canvasPageXY.y)};
  if (!show ||
      !(this.stackedGraph_ || this.stackedGraphOther_) ||
      p.x < coord.xMinValue() || coord.xMaxValue() < p.x ||
      p.y < coord.yMinValue() || coord.yMaxValue() < p.y) {
    this.hoveringInfo_.show(false);
    return;
  } else {
    this.hoveringInfo_.show(true);
  }

  /**
   * Finds the closest lines (upside and downside of the cursor position).
   * Returns a set of upside/downside line indices and point index on success
   * or null.
   */
  function findClosestLines(lines, opt_startIndex, opt_endIndex) {
    var offsetIndex = opt_startIndex || 0;
    lines =
        opt_endIndex != null ? lines.slice(offsetIndex, opt_endIndex) :
        opt_startIndex != null ? lines.slice(offsetIndex) :
        lines;

    var upsideClosestLineIndex = null;
    var upsideClosestYDistance = coord.yValueRange();
    var downsideClosestLineIndex = null;
    var downsideClosestYDistance = coord.yValueRange();
    var upsideClosestPointIndex = null;

    for (var lineIndex = 0, line; line = lines[lineIndex]; ++lineIndex) {
      for (var i = 1; line[i]; ++i) {
        var p0 = line[i - 1], p1 = line[i];
        if (p0[0] <= p.x && p.x < p1[0]) {
          // Calculate y-value of the line at p.x, which is the cursor point.
          var y = (p.x - p0[0]) / (p1[0] - p0[0]) * (p1[1] - p0[1]) + p0[1];
          if (p.y < y && y - p.y < upsideClosestYDistance) {
            upsideClosestLineIndex = lineIndex;
            upsideClosestYDistance = y - p.y;

            if (p.x - p0[0] < p1[0] - p.x) {
              upsideClosestPointIndex = i - 1;
            } else {
              upsideClosestPointIndex = i;
            }
          } else if (y <= p.y && p.y - y < downsideClosestYDistance) {
            downsideClosestLineIndex = lineIndex;
            downsideClosestYDistance = p.y - y;
          }
          break;
        }
      }
    }

    return (upsideClosestLineIndex != null &&
            upsideClosestPointIndex != null) ?
        {'upsideLineIndex': offsetIndex + upsideClosestLineIndex,
         'downsideLineIndex': downsideClosestYDistance == null ? null :
                              offsetIndex + downsideClosestLineIndex,
         'upsidePointIndex': offsetIndex + upsideClosestPointIndex} :
        null;
  }

  // Find the closest lines above and below the mouse cursor.
  var closest = null;
  // Since the other set of graphs are drawn over the first set, try to find
  // the closest lines from the other set of graphs first.
  if (this.graphsOtherStartIndex_ && this.stackedGraphOther_) {
    closest = findClosestLines(this.plotData_, this.graphsOtherStartIndex_);
  }
  if (!closest && this.stackedGraph_) {
    closest = this.graphsOtherStartIndex_ ?
        findClosestLines(this.plotData_, 0, this.graphsOtherStartIndex_) :
        findClosestLines(this.plotData_);
  }
  if (!closest) {
    this.hoveringInfo_.show(false);
    return;
  }

  // Update the contents of the hovering info box.
  // Color indicator, description and the value of the item.
  this.hoveringInfo_.setColorIndicator(
    this.getDataColor(closest.upsideLineIndex));
  this.hoveringInfo_.setLegendText(
    this.dataDescriptions_[closest.upsideLineIndex]);
  var y1 = this.plotData_[closest.upsideLineIndex][closest.upsidePointIndex][1];
  var y0 = closest.downsideLineIndex == null ?
      0 :
      this.plotData_[closest.downsideLineIndex][closest.upsidePointIndex][1];
  this.hoveringInfo_.setItemValue(y1 - y0);

  // Locate the hovering info box near the mouse cursor.
  var DIV_X_OFFSET = 10, DIV_Y_OFFSET = -20;
  if (evtPageXY.x + this.hoveringInfo_.getElement().offsetWidth <
      canvasPageXY.x + this.canvasElement_.offsetWidth) {
    this.hoveringInfo_.locateAtPageXY(evtPageXY.x + DIV_X_OFFSET,
                                      evtPageXY.y + DIV_Y_OFFSET);
  } else {  // If lacking space at the right side, locate it at the left side.
    this.hoveringInfo_.locateAtPageXY(
      evtPageXY.x - this.hoveringInfo_.getElement().offsetWidth - DIV_X_OFFSET,
      evtPageXY.y + DIV_Y_OFFSET);
  }
};

/**
 * Handle a mouse move event.
 *
 * @param {Object} evt A mouse event object representing a mouse move event.
 */
Plotter.prototype.onMouseMove_ = function(evt) {
  var self = this;

  var canvas = evt.currentTarget.firstChild;
  var evtPageXY = domUtils.pageXYOfEvent(evt);
  var canvasPageXY = domUtils.pageXY(this.canvasElement_);
  var positionX = evtPageXY.x - canvasPageXY.x;
  var positionY = evtPageXY.y - canvasPageXY.y;

  // Identify the index of the x value that is closest to the mouse x value.
  var xValue = this.coordinates.xValue(positionX);
  var lineIndex = !this.stackedGraph_ ? 0 :
      this.graphsOtherStartIndex_ ? this.graphsOtherStartIndex_ - 1 :
      this.plotData_.length - 1;
  var line = this.plotData_[lineIndex];
  var min_diff = Math.abs(line[0][0] - xValue);
  indexValueX = 0;
  for (var i = 1; i < line.length; ++i) {
    var diff = Math.abs(line[i][0] - xValue);
    if (diff < min_diff) {
      min_diff = diff;
      indexValueX = i;
    }
  }

  // Identify the index of the x value closest to the mouse x value for the
  // other graph being overlayed on top of the original graph, if one exists.
  if (this.unitsYOther_) {
    var xValue = this.coordinatesOther.xValue(positionX);
    var lineIndexOther = !this.stackedGraphOther_ ?
        this.graphsOtherStartIndex_ : this.plotData_.length - 1;
    var lineOther = this.plotData_[lineIndexOther];
    var min_diff = Math.abs(lineOther[0][0] - xValue);
    var indexValueXOther = 0;
    for (var i = 1; i < lineOther.length; ++i) {
      var diff = Math.abs(lineOther[i][0] - xValue);
      if (diff < min_diff) {
        min_diff = diff;
        indexValueXOther = i;
      }
    }
  }

  // Update coordinate information displayed directly underneath the graph.
  function legendLabel(lineIndex, opt_labelText) {
    return '<span style="color:' + self.getDataColor(lineIndex) + '">' +
        (opt_labelText || self.dataDescriptions_[lineIndex]) +
        '</span>:&nbsp;&nbsp;';
  }
  function valuesAtCursor(lineIndex, pointIndex, unitsY, yValue) {
    return '<span style="color:' + self.getDataColor(lineIndex) + '">' +
        self.plotData_[lineIndex][pointIndex][0] + ' ' + self.unitsX_ + ': ' +
        addCommas(self.plotData_[lineIndex][pointIndex][1].toFixed(2)) + ' ' +
        unitsY  + '</span> [hovering at ' + addCommas(yValue.toFixed(2)) +
        ' ' + unitsY + ']';
  }

  this.infoBox_.rows[0].label.innerHTML = legendLabel(lineIndex);
  this.infoBox_.rows[0].content.innerHTML = valuesAtCursor(
    lineIndex, indexValueX, this.unitsY_, this.coordinates.yValue(positionY));
  var row = this.infoBox_.rows[1];
  if (this.unitsYOther_) {
    row.label.innerHTML = legendLabel(lineIndexOther);
    row.content.innerHTML = valuesAtCursor(
      lineIndexOther, indexValueXOther, this.unitsYOther_,
      this.coordinatesOther.yValue(positionY));
  } else if (this.graphsOtherStartIndex_) {
    row.label.innerHTML = legendLabel(
      this.stackedGraphOther_ ?
          this.plotData_.length - 1 : this.graphsOtherStartIndex_);
    row.content.innerHTML = valuesAtCursor(
      this.stackedGraphOther_ ?
          this.plotData_.length - 1 : this.graphsOtherStartIndex_,
      indexValueX, this.unitsY_, this.coordinates.yValue(positionY));
  } else if (!this.stackedGraph_ && this.dataDescriptions_.length > 1) {
    row.label.innerHTML = legendLabel(1);
    row.content.innerHTML = valuesAtCursor(
      1, indexValueX, this.unitsY_, this.coordinates.yValue(positionY));
  } else if (row) {
    row.label.innerHTML = '';
    row.content.innerHTML = '';
  }

  // If there is a horizontal marker, also display deltas relative to it.
  if (this.horizontal_marker_) {
    var baseline = this.horizontal_marker_.value;
    var delta = this.coordinates.yValue(positionY) - baseline;
    var fraction = delta / baseline;  // Allow division by 0.

    var deltaStr = (delta >= 0 ? '+' : '') + delta.toFixed(0) + ' ' +
        this.unitsY_;
    var percentStr = (fraction >= 0 ? '+' : '') + (fraction * 100).toFixed(3) +
        '%';

    this.baselineDeltasTd_.innerHTML = deltaStr + ': ' + percentStr;

    if (this.unitsYOther_) {
      var baseline = this.horizontal_marker_.otherValue;
      var yValue2 = this.coordinatesOther.yValue(positionY);
      var delta = yValue2 - baseline;
      var fraction = delta / baseline;  // Allow division by 0.

      var deltaStr = (delta >= 0 ? '+' : '') + delta.toFixed(0) + ' ' +
          this.unitsYOther_;
      var percentStr = (fraction >= 0 ? '+' : '') +
          (fraction * 100).toFixed(3) + '%';
      this.baselineDeltasTd_.innerHTML += '<br>' + deltaStr + ': ' + percentStr;
    }
  }

  this.updateRuler_(evt);
  this.updateCursor_(this.coordinates, indexValueX, this.cursorDiv_, 0);
  if (this.unitsYOther_ && this.graphsOtherStartIndex_) {
    this.updateCursor_(this.coordinatesOther, indexValueXOther,
                       this.cursorDivOther_, this.graphsOtherStartIndex_);
  }

  // If there are events displayed, see if we're hovering close to an existing
  // event on the graph, and if so, display the metadata associated with it.
  if (this.eventName_ != null && this.eventInfo_ != null) {
    this.infoBox_.rows[1].label.innerHTML = 'Event "' + this.eventName_ +
        '":&nbsp;&nbsp;';
    var data = this.eventInfo_;
    var showed_event = false;
    var x = 0;
    for (var index = 0; index < data.length; ++index) {
      var event_time = data[index][0];
      x = this.coordinates.xPixel(event_time);
      if (positionX >= x - 10 && positionX <= x + 10) {
        var metadata = data[index][1];
        var metadata_str = "";
        for (var meta_key in metadata)
          metadata_str += meta_key + ': ' + metadata[meta_key] + ', ';
        metadata_str = metadata_str.substring(0, metadata_str.length - 2);
        this.infoBox_.rows[1].content.innerHTML = event_time + ' ' +
            this.unitsX_ + ': {' + metadata_str + '}';
        showed_event = true;
        this.updateEventDiv_(x, true);
        break;
      }
    }
    if (!showed_event) {
      this.coordinatesTdOther_.innerHTML =
          'move mouse close to vertical event marker';
      this.updateEventDiv_(x, false);
    }
  }

  this.updateHoveringInfo_(evt, true);
};

/**
 * Handle a mouse over event.
 *
 * @param {Object} evt A mouse event object representing a mouse move event.
 */
Plotter.prototype.onMouseOver_ = function(evt) {
  this.updateHoveringInfo_(evt, true);
};

/**
 * Handle a mouse out event.
 *
 * @param {Object} evt A mouse event object representing a mouse move event.
 */
Plotter.prototype.onMouseOut_ = function(evt) {
  this.updateHoveringInfo_(evt, false);
};

/**
 * Handle a mouse click event.
 *
 * @param {Object} evt A mouse event object representing a mouse click event.
 */
Plotter.prototype.onMouseClick_ = function(evt) {
  // Shift-click controls the horizontal reference line.
  if (evt.shiftKey) {
    if (this.horizontal_marker_)
      this.horizontal_marker_.remove();

    var canvasY = domUtils.pageXYOfEvent(evt).y -
                  domUtils.pageXY(this.canvasElement_).y;
    this.horizontal_marker_ = new HorizontalMarker(
        this.canvasElement_,
        this.coordinates.yValue(canvasY),
        (this.coordinatesOther ? this.coordinatesOther.yValue(canvasY) : null));
    // Insert before cursor node, otherwise it catches clicks.
    this.cursorDiv_.parentNode.insertBefore(
        this.horizontal_marker_.markerDiv, this.cursorDiv_);
    this.horizontal_marker_.locateAt(this.canvasElement_, canvasY);
  }
};

/**
 * Generates and returns a list of div objects representing horizontal lines in
 * the graph that indicate y-axis values at a computed interval.
 *
 * @param {Object} coordinateSystem a Coordinates object representing the
 *     coordinate system for which the graduations should be created.
 * @param {number} colorIndex An index into the |this.colors| array representing
 *     the color to make the graduations in the event that two graphs with
 *     different coordinate systems are being overlayed on the same plot.
 * @param {boolean} isRightSide Whether or not the graduations should have
 *     right-aligned text (used when the graduations are for a second graph
 *     that is being overlayed on top of another graph).
 * @return {Array} An array of DOM Element objects representing the divs.
 */
Plotter.prototype.graduations_ = function(coordinateSystem, colorIndex,
                                          isRightSide) {
  // Don't allow a graduation in the bottom 5% of the chart or the number label
  // would overflow the chart bounds.
  var yMin = coordinateSystem.yLowerLimitValue() +
      .05 * coordinateSystem.yValueRange();
  var yRange = coordinateSystem.yUpperLimitValue() - yMin;

  // Use the largest scale that fits 3 or more graduations.
  // We allow scales of [...,500, 250, 100, 50, 25, 10,...].
  var scale = 5000000000;
  while (scale) {
    if (Math.floor(yRange / scale) > 2) break;  // 5s.
    scale /= 2;
    if (Math.floor(yRange / scale) > 2) break;  // 2.5s.
    scale /= 2.5;
    if (Math.floor(yRange / scale) > 2) break;  // 1s.
    scale /= 2;
  }

  var graduationPosition = yMin + (scale - yMin % scale);
  var graduationDivs = [];
  while (graduationPosition < coordinateSystem.yUpperLimitValue() ||
         yRange == 0) {
    var graduation = document.createElement('div');
    var canvasPosition;
    if (yRange == 0) {
      // Center the graduation vertically.
      canvasPosition = this.canvasElement_.offsetHeight / 2;
    } else {
      canvasPosition = coordinateSystem.yPixel(graduationPosition);
    }
    if (this.unitsYOther_) {
      graduation.style.borderTop = '1px dashed ' +
          this.makeColorTransparent(colorIndex, 0.4)
    } else {
      graduation.style.borderTop = '1px dashed rgba(0,0,0,.08)';
    }
    graduation.style.position = 'absolute';
    graduation.style.left = this.canvasElement_.offsetLeft + 'px';
    graduation.style.top = canvasPosition + this.canvasElement_.offsetTop +
        'px';
    graduation.style.width = this.canvasElement_.offsetWidth -
        this.canvasElement_.offsetLeft + 'px';
    graduation.style.paddingLeft = '4px';
    if (this.unitsYOther_)
      graduation.style.color = this.makeColorTransparent(colorIndex, 0.9)
    else
      graduation.style.color = 'rgba(0,0,0,.4)';
    graduation.style.fontSize = '9px';
    graduation.style.paddingTop = '0';
    graduation.style.zIndex = '-1';
    if (isRightSide)
      graduation.style.textAlign = 'right';
    if (yRange == 0)
      graduation.innerHTML = addCommas(yMin);
    else
      graduation.innerHTML = addCommas(graduationPosition);
    graduationDivs.push(graduation);
    if (yRange == 0)
      break;
    graduationPosition += scale;
  }
  return graduationDivs;
};

/**
 * Generates and returns a div object representing the horizontal line that
 * follows the mouse pointer around the plot.
 *
 * @return {Object} A DOM Element object representing the div.
 */
Plotter.prototype.ruler_ = function() {
  var ruler = document.createElement('div');
  ruler.setAttribute('class', 'plot-ruler');
  ruler.style.borderBottom = '1px dotted black';
  ruler.style.position = 'absolute';
  ruler.style.left = '-2px';
  ruler.style.top = '-2px';
  ruler.style.width = '0px';
  ruler.style.height = '0px';
  return ruler;
};

/**
 * Generates and returns a canvas object representing the plot itself.
 *
 * @return {Object} A DOM Element object representing the canvas.
 */
Plotter.prototype.canvas_ = function() {
  var canvas = document.createElement('canvas');
  canvas.setAttribute('id', '_canvas');
  canvas.setAttribute('class', 'plot');
  canvas.setAttribute('width', this.coordinates.widthMax);
  canvas.setAttribute('height', this.coordinates.heightMax);
  canvas.plotter = this;
  return canvas;
};

/**
 * Generates and returns a div object representing the coordinate information
 * displayed directly underneath a graph.
 *
 * @return {Object} A DOM Element object representing the div.
 */
Plotter.prototype.coordinates_ = function() {
  var coordinatesDiv = document.createElement('div');
  var table_html = '<table border=0 width="100%"';
  if (this.is_lookout_) {
    table_html += ' style="font-size:0.8em"';
  }
  table_html += '><tbody><tr>';
  table_html += '<td><span class="legend_item"></span>' +
      '<span class="plot-coordinates"><i>move mouse over graph</i></span></td>';
  table_html += '<td align="right">x-axis is ' + this.unitsX_ + '</td>';
  table_html += '</tr><tr>';
  table_html += '<td><span class="legend_item"></span>' +
      '<span class="plot-coordinates"></span></td>';

  if (!this.is_lookout_) {
    table_html += '<td align="right" style="color: ' + HorizontalMarker.COLOR +
        '"><i>Shift-click to place baseline.</i></td>';
  }
  table_html += '</tr></tbody></table>';
  coordinatesDiv.innerHTML = table_html;

  var trs = coordinatesDiv.querySelectorAll('tr');
  this.infoBox_ = {rows: []};
  this.infoBox_.rows.push({
    label: trs[0].querySelector('span.legend_item'),
    content: trs[0].querySelector('span.plot-coordinates')});
  if (this.dataDescriptions_.length > 1 || this.eventName_) {
    this.infoBox_.rows.push({
      label: trs[1].querySelector('span.legend_item'),
      content: trs[1].querySelector('span.plot-coordinates')});
  }

  this.baselineDeltasTd_ = trs[1].childNodes[1];

  // Add a summary of legends in case of stacked graphs.
  if (this.stackedGraph_ || this.stackedGraphOther_) {
    var legendPane = document.createElement('div');
    legendPane.style.fontSize = '80%';
    coordinatesDiv.appendChild(legendPane);

    if (this.graphsOtherStartIndex_) {
      legendPane.appendChild(
        this.createLegendsSummaryElement_(
          this.dataDescriptions_.slice(0, this.graphsOtherStartIndex_),
          0));
      legendPane.appendChild(
        this.createLegendsSummaryElement_(
          this.dataDescriptions_.slice(this.graphsOtherStartIndex_),
          this.graphsOtherStartIndex_));
    } else {
      legendPane.appendChild(
        this.createLegendsSummaryElement_(this.dataDescriptions_, 0));
    }
  }

  return coordinatesDiv;
};

/**
 * Creates and returns a DOM element which shows a summary of legends.
 *
 * @param {!Array.<string>} legendTexts An array of legend texts.
 * @param {number} colorIndexOffset Offset index for color.  i-th legend text
 *     has an indicator in {@code (colorIndexOffset + i)}-th color
 * @return {!Element} An element which shows a summary of legends.
 */
Plotter.prototype.createLegendsSummaryElement_ = function(legendTexts,
                                                          colorIndexOffset) {
  var containerElem = document.createElement('div');

  for (var i = 0, text; text = legendTexts[i]; ++i) {
    var colorIndicatorElem = document.createElement('div');
    colorIndicatorElem.style.display = 'inline-block';
    colorIndicatorElem.style.width = '1em';
    colorIndicatorElem.style.height = '1em';
    colorIndicatorElem.style.verticalAlign = 'text-bottom';
    colorIndicatorElem.style.margin = '0 0.24em 0 0';
    colorIndicatorElem.style.border = '1px solid #000';
    colorIndicatorElem.style.backgroundColor =
        this.getDataColor(colorIndexOffset + i);
    var legendTextElem = document.createElement('span');
    legendTextElem.textContent = text;
    var legendElem = document.createElement('span');
    legendElem.style.whiteSpace = 'nowrap';
    legendElem.appendChild(colorIndicatorElem);
    legendElem.appendChild(legendTextElem);
    legendElem.style.margin = '0 0.8em 0 0';
    containerElem.appendChild(legendElem);
    // Add a space to break lines if necessary.
    containerElem.appendChild(document.createTextNode(' '));
  }

  return containerElem;
};
