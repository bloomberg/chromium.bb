/*
  Copyright (c) 2012 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

/**
 * @fileoverview Class and functions to handle positioning of plot data points.
 */

/**
 * Class that handles plot data positioning.
 * @constructor
 *
 * @param {Array} plotData Data that will be plotted.  It is an array of lines,
 *     where each line is an array of points, and each point is a length-2 array
 *     representing an (x, y) pair.
 */
function Coordinates(plotData) {
  this.plotData = plotData;

  height = window.innerHeight - 16;
  width = window.innerWidth - 16;

  this.widthMax = width;
  this.heightMax = Math.min(400, height - 85);

  this.processValues_('x');
  this.processValues_('y');
}

/**
 * Determines the min/max x or y values in the plot, accounting for some extra
 * buffer space.
 *
 * @param {string} type The type of value to process, either 'x' or 'y'.
 */
Coordinates.prototype.processValues_ = function (type) {
  var merged = [];
  for (var i = 0; i < this.plotData.length; i++)
    for (var j = 0; j < this.plotData[i].length; j++) {
      if (type == 'x')
        merged.push(parseFloat(this.plotData[i][j][0]));  // Index 0 is x value.
      else
        merged.push(parseFloat(this.plotData[i][j][1]));  // Index 1 is y value.
    }

  min = merged[0];
  max = merged[0];
  for (var i = 1; i < merged.length; ++i) {
    if (isNaN(min) || merged[i] < min)
      min = merged[i];
    if (isNaN(max) || merged[i] > max)
      max = merged[i];
  }

  var bufferSpace = 0.02 * (max - min);

  if (type == 'x') {
    this.xBufferSpace_ = bufferSpace;
    this.xMinValue_ = min;
    this.xMaxValue_ = max;
  } else {
    this.yBufferSpace_ = bufferSpace;
    this.yMinValue_ = min;
    this.yMaxValue_ = max;
  }
};

/**
 * Difference between horizontal upper and lower limit values.
 *
 * @return {number} The x value range.
 */
Coordinates.prototype.xValueRange = function() {
  return this.xUpperLimitValue() - this.xLowerLimitValue();
};

/**
 * Difference between vertical upper and lower limit values.
 *
 * @return {number} The y value range.
 */
Coordinates.prototype.yValueRange = function() {
  return this.yUpperLimitValue() - this.yLowerLimitValue();
};

/**
 * Converts horizontal data value to pixel value on canvas.
 *
 * @param {number} value The x data value.
 * @return {number} The corresponding x pixel value on the canvas.
 */
Coordinates.prototype.xPixel = function(value) {
  return this.widthMax *
      ((value - this.xLowerLimitValue()) / this.xValueRange());
};

/**
 * Converts vertical data value to pixel value on canvas.
 *
 * @param {number} value The y data value.
 * @return {number} The corresponding y pixel value on the canvas.
 */
Coordinates.prototype.yPixel = function(value) {
  if (this.yValueRange() == 0) {
    // Completely horizontal lines should be centered horizontally.
    return this.heightMax / 2;
  } else {
    return this.heightMax -
        (this.heightMax *
         (value - this.yLowerLimitValue()) / this.yValueRange());
  }
};

/**
 * Converts x point on canvas to data value it represents.
 *
 * @param {number} position The x pixel value on the canvas.
 * @return {number} The corresponding x data value.
 */
Coordinates.prototype.xValue = function(position) {
  return this.xLowerLimitValue() +
      (position / this.widthMax * this.xValueRange());
};

/**
 * Converts y point on canvas to data value it represents.
 *
 * @param {number} position The y pixel value on the canvas.
 * @return {number} The corresponding y data value.
 */
Coordinates.prototype.yValue = function(position) {
  var ratio = this.heightMax / (this.heightMax - position);
  return this.yLowerLimitValue() + (this.yValueRange() / ratio);
};

/**
 * Returns the minimum x value of all the data points.
 *
 * @return {number} The minimum x value of all the data points.
 */
Coordinates.prototype.xMinValue = function() {
  return this.xMinValue_;
};

/**
 * Returns the maximum x value of all the data points.
 *
 * @return {number} The maximum x value of all the data points.
 */
Coordinates.prototype.xMaxValue = function() {
  return this.xMaxValue_;
};

/**
 * Returns the minimum y value of all the data points.
 *
 * @return {number} The minimum y value of all the data points.
 */
Coordinates.prototype.yMinValue = function() {
  return this.yMinValue_;
};

/**
 * Returns the maximum y value of all the data points.
 *
 * @return {number} The maximum y value of all the data points.
 */
Coordinates.prototype.yMaxValue = function() {
  return this.yMaxValue_;
};

/**
 * Returns the x value at the lower limit of the bounding box of the canvas.
 *
 * @return {number} The x value at the lower limit of the bounding box of
 *     the canvas.
 */
Coordinates.prototype.xLowerLimitValue = function() {
  return this.xMinValue_ - this.xBufferSpace_;
};

/**
 * Returns the x value at the upper limit of the bounding box of the canvas.
 *
 * @return {number} The x value at the upper limit of the bounding box of
 *     the canvas.
 */
Coordinates.prototype.xUpperLimitValue = function() {
  return this.xMaxValue_ + this.xBufferSpace_;
};

/**
 * Returns the y value at the lower limit of the bounding box of the canvas.
 *
 * @return {number} The y value at the lower limit of the bounding box of
 *     the canvas.
 */
Coordinates.prototype.yLowerLimitValue = function() {
  return this.yMinValue_ - this.yBufferSpace_;
};

/**
 * Returns the y value at the upper limit of the bounding box of the canvas.
 *
 * @return {number} The y value at the upper limit of the bounding box of
 *     the canvas.
 */
Coordinates.prototype.yUpperLimitValue = function() {
  return this.yMaxValue_ + this.yBufferSpace_;
};
