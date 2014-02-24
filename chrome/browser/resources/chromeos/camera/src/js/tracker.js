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
   * @type {Array.<camera.Tracker.Face>}
   * @private
   */
  this.faces_ = [new camera.Tracker.Face(0.5, 0.5, 0.3, 0.3, 1)];

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

  // Remember the dimensions, since the input canvas may change, before the
  // returned callback is called.
  var width = this.input_.width;
  var height = this.input_.height;

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

      var newFaces = [];
      for (var index = 0; index < result.length; index++) {
        var face = new camera.Tracker.Face(
            result[index].x / width,
            result[index].y / height,
            result[index].width / width,
            result[index].height / height,
            result[index].confidence);
        newFaces.push(face);
      }

      // Copy the array, since it will be modified while matching with new
      // faces.
      var oldFaces = this.faces_.slice();

      // Try to map the new faces to the previous ones. Map up to the number of
      // faces detected previously.
      var mappedFaces = [];
      while (newFaces.length && oldFaces.length) {
        var minDistNewFace;
        var minDistOldFace;
        var minDist = Number.MAX_VALUE;
        for (var i = 0; i < newFaces.length; i++) {
          var newFace = newFaces[i];
          for (var j = 0; j < oldFaces.length; j++) {
            var oldFace = oldFaces[j];
            // Euclidean metric. No need to do a square root for comparison.
            var distance =
                Math.pow(oldFace.x - newFace.x, 2) +
                Math.pow(oldFace.y - newFace.y, 2) +
                Math.pow(oldFace.width - newFace.width, 2) +
                Math.pow(oldFace.height - newFace.height, 2);
            if (distance < minDist) {
              minDist = distance;
              minDistNewFace = newFace;
              minDistOldFace = oldFace;
            }
          }
        }
        // Update the old face to point to new coordinates.
        minDistOldFace.targetX = minDistNewFace.x;
        minDistOldFace.targetY = minDistNewFace.y;
        minDistOldFace.targetWidth = minDistNewFace.width;
        minDistOldFace.targetHeight = minDistNewFace.height;
        minDistOldFace.targetConfidence = minDistNewFace.confidence;
        mappedFaces.push(minDistOldFace);
        // Remove the matched faces from both arrays.
        newFaces.splice(newFaces.indexOf(minDistNewFace), 1);
        oldFaces.splice(oldFaces.indexOf(minDistOldFace), 1);
      }

      // If there is more new faces than previous ones, then just append them.
      for (var index = 0; index < newFaces.length; index++) {
        mappedFaces.push(newFaces[index]);
      }

      // Sort the mapped faces, so the best one is first on the list.
      mappedFaces.sort(function(a, b) {
        return a.confidence < b.confidence;
      });

      // Ensure that there is at least one face. If not, then add the best old
      // one.
      if (!mappedFaces.length)
        mappedFaces.push(this.faces_[0]);

      // Swap the container with faces with the new one.
      this.faces_ = mappedFaces;
    }
    this.busy_ = false;
    finishMeasuring();
  }.bind(this));
};

/**
 * Updates the face by applying some interpolation.
 */
camera.Tracker.prototype.update = function() {
  for (var index = 0; index < this.faces_.length; index++) {
    this.faces_[index].update();
  }
};

/**
 * Returns detected faces by the last call of update(), mapped to the canvas
 * coordinates. Will always return at least one face.
 *
 * @param {Canvas} canvas Canvas used to map the face coordinates onto.
 * @return {Array.<camera.Tracker.Face>} Detected faces' objects.
 */
camera.Tracker.prototype.getFacesForCanvas = function(canvas) {
  var result = [];
  for (var index = 0; index < this.faces_.length; index++) {
    var inputFace = this.faces_[index];

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
    result.push(outputFace);
  }

  return result;
};

