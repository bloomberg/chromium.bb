// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for effects.
 */
camera.effects = camera.effects || {};

/**
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.Cinema = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.mode_ = 0;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.Cinema.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Cinema.prototype.randomize = function() {
  // TODO(mtomasz): To be implemented.
  this.mode_ = (this.mode_ + 1) % 3;
};

/**
 * @override
 */
camera.effects.Cinema.prototype.filterFrame = function(canvas, faces) {
  var face = faces[0];
  var x = canvas.width * (face.x + (face.width / 2));
  var y = canvas.height * face.y * 1.5;
  var radius = Math.sqrt(Math.pow(face.width * canvas.width, 2) +
                         Math.pow(face.height * canvas.height, 2));
  canvas.tiltShift(0,
                   y,
                   canvas.width - 1,
                   y,
                   radius / 15.0,
                   canvas.height * 1.1);
  canvas.brightnessContrast(0.1, 0.2).
      vibrance(-1).
      vignette(0.5, 0.4);
};

/**
 * @override
 */
camera.effects.Cinema.prototype.getTitle = function() {
  return chrome.i18n.getMessage('cinemaEffect');
};

/**
 * @override
 */
camera.effects.Cinema.prototype.isSlow = function() {
  return true;
};

/**
 * @override
 */
camera.effects.Cinema.prototype.usesHeadTracker = function() {
  return true;
};

