// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Detects and tracks faces on the input stream.
 *
 * @param {Canvas} input Input canvas.
 * @constructor
 */
camera.Tracker = function(input) {
  /**
   * @type {Canvas}
   * @private
   */
  this.input_ = input;

  /**
   * @type {camera.Tracker.Face}
   * @private
   */
  this.face_ = new camera.Tracker.Face(0.5, 0.5, 0.3, 0.3, 1);

  /**
   * @type {boolean}
   * @private
   */
  this.busy_ = false;

  /**
   * @type {camera.util.PerformanceMonitor}
   * @private
   */
  this.performanceMonitor_ = new camera.util.PerformanceMonitor();

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Represents a detected face.
 *
 * @param {number} x Initial x coordinate, between 0 and 1.
 * @param {number} y Initial y coordinate, between 0 and 1.
 * @param {number} width Initial width, between 0 and 1.
 * @param {number} height Initial height, between 0 and 1.
 * @param {number} confidence Initial confidence, between 0 and 1.
 * @constructor
 */
camera.Tracker.Face = function(x, y, width, height, confidence) {
  /**
   * @type {number}
   * @private
   */
  this.x_ = x;

  /**
   * @type {number}
   * @private
   */
  this.y_ = y;

  /**
   * @type {number}
   * @private
   */
  this.targetX_ = x;

  /**
   * @type {number}
   * @private
   */
  this.targetY_ = y;

  /**
   * @type {number}
   * @private
   */
  this.width_ = width;

  /**
   * @type {number}
   * @private
   */
  this.height_ = height;

  /**
   * @type {number}
   * @private
   */
  this.targetWidth_ = width;

  /**
   * @type {number}
   * @private
   */
  this.targetHeight_ = height;

  /**
   * @type {number}
   * @private
   */
  this.confidence_ = confidence;

  /**
   * @type {number}
   * @private
   */
  this.targetConfidence_ = confidence;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.Tracker.Face.prototype = {
  /**
   * Returns the current interpolated x coordinate of the head.
   * @return {number} Position as fraction (0-1) of the width.
   */
  get x() {
    return this.x_;
  },

  /**
   * Returns the current interpolated y coordinate of the head.
   * @return {number} Position as fraction (0-1) of the width.
   */
  get y() {
    return this.y_;
  },

  /**
   * Returns the current interpolated width of the head.
   * @return {number} Width as fraction (0-1) of the image's width.
   */
  get width() {
    return this.width_;
  },

  /**
   * Returns the current interpolated height of the head.
   * @return {number} Height as fraction (0-1) of the image's height.
   */
  get height() {
    return this.height_;
  },

  /**
   * Returns the current interpolated confidence of detection.
   * @return {number} Confidence level.
   */
  get confidence() {
    return this.confidence_;
  },

  /**
   * Sets the target x coordinate of the face.
   * @param {number} x Position as a fraction of the width (0-1).
   */
  set targetX(x) {
    this.targetX_ = x;
  },

  /**
   * Sets the target y coordinate of the face.
   * @param {number} y Position as a fraction of the height (0-1).
   */
  set targetY(y) {
    this.targetY_ = y;
  },

  /**
   * Sets the target width of the face.
   * @param {number} width Width as a fraction of the image's width (0-1).
   */
  set targetWidth(width) {
    this.targetWidth_ = width;
  },

  /**
   * Sets the target height of the face.
   * @param {number} height Height as a fraction of the image's height (0-1).
   */
  set targetHeight(height) {
    this.targetHeight_ = height;
  },

  /**
   * Sets the target confidence level.
   * @param {number} Confidence level.
   */
  set targetConfidence(confidence) {
    this.targetConfidence_ = confidence;
  }
};

/**
 * Updates the detected face by applying some interpolation.
 */
camera.Tracker.Face.prototype.update = function() {
  var step = 0.3;
  this.x_ += (this.targetX_ - this.x_) * step;
  this.y_ += (this.targetY_ - this.y_) * step;
  this.width_ += (this.targetWidth_ - this.width_) * step;
  this.height_ += (this.targetHeight_ - this.height_) * step;
  this.confidence_ += (this.targetConfidence_ - this.confidence_) * step;
};

camera.Tracker.prototype = {
  /**
   * Returns number of frames analyzed per second (without interpolating).
   * @return {number}
   */
  get fps() {
    return this.performanceMonitor_.fps;
  },

  /**
   * Returns true if busy, false otherwise.
   * @return {boolean}
   */
  get busy() {
    return this.busy_;
  }
};

/**
 * Starts the tracker. Note, that detect() and update() still need to be called.
 */
camera.Tracker.prototype.start = function() {
  this.performanceMonitor_.start();
};

/**
 * Stops the tracker.
 */
camera.Tracker.prototype.stop = function() {
  this.performanceMonitor_.stop();
};

/**
 * Requests face detection on the current frame.
 */
camera.Tracker.prototype.detect = function() {
  if (this.busy_)
    return;
  this.busy_ = true;
  var finishMeasuring = this.performanceMonitor_.startMeasuring();

  var result = ccv.detect_objects({
    canvas: this.input_,
    interval: 5,
    min_neighbors: 1,
    worker: 1,
    async: true
  })(function(result) {
    if (result.length) {
      result.sort(function(a, b) {
        return a.confidence < b.confidence;
      });

      this.face_.targetX = result[0].x / this.input_.width;
      this.face_.targetY = result[0].y / this.input_.height;
      this.face_.targetWidth = result[0].width / this.input_.width;
      this.face_.targetHeight = result[0].height / this.input_.height;
      this.face_.targetConfidence = 1;
    }
    this.busy_ = false;
    finishMeasuring();
  }.bind(this));
};

/**
 * Updates the face by applying some interpolation.
 */
camera.Tracker.prototype.update = function() {
  this.face_.update();
};

/**
 * Returns detected faces by the last call of update(), mapped to the canvas
 * coordinates.
 *
 * @param {Canvas} canvas Canvas used to map the face coordinates onto.
 * @return {camera.Tracker.Face} Detected face object.
 */
camera.Tracker.prototype.getFaceForCanvas = function(canvas) {
  var inputFace = this.face_;

  var inputAspect = this.input_.width / this.input_.height;
  var outputAspect = canvas.width / canvas.height;
  var scaleWidth = inputAspect / outputAspect;

  var outputX = (inputFace.x - 0.5) * scaleWidth + 0.5;
  var outputWidth = inputFace.width * scaleWidth;

  var outputFace = new camera.Tracker.Face(outputX,
                                           inputFace.y,
                                           outputWidth,
                                           inputFace.height,
                                           inputFace.confidence);
  return outputFace;
};

