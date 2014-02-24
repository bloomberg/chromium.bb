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
 * @param {number} offset Vertical offset in percents.
 * @param {number} size Size of the pinch.
 * @param {number} strength Strength of the pinch.
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.Pinch = function(offset, size, strength) {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.amount_ = 0.5;

  /**
   * @type {number}
   * @private
   */
  this.offset_ = offset;

  /**
   * @type {number}
   * @private
   */
  this.size_ = size;

  /**
   * @type {number}
   * @private
   */
  this.strength_ = strength;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.Pinch.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Pinch.prototype.randomize = function() {
  this.amount_ = Math.random() * 0.6 + 0.2;
};

/**
 * @override
 */
camera.effects.Pinch.prototype.filterFrame = function(canvas, faces) {
  for (var index = 0; index < faces.length; index++) {
    var x = canvas.width * (faces[index].x + (faces[index].width / 2));
    var y = canvas.height * faces[index].y;
    var radius = Math.sqrt(Math.pow(faces[index].width * canvas.width, 2) +
                           Math.pow(faces[index].height * canvas.height, 2));
    canvas.bulgePinch(x,
                      y + this.offset_ * faces[index].height * canvas.height,
                      radius * this.amount_ * this.size_,
                      0.5 * this.strength_);
  }
};

/**
 * @override
 */
camera.effects.Pinch.prototype.usesHeadTracker = function() {
  return true;
};

/**
 * @constructor
 * @extends {camera.effects.Pinch}
 */
camera.effects.BigHead = function() {
  camera.effects.Pinch.call(this, 0, 1.0, 1.0);
};

camera.effects.BigHead.prototype = {
  __proto__: camera.effects.Pinch.prototype
};

/**
 * @override
 */
camera.effects.BigHead.prototype.getTitle = function() {
  return chrome.i18n.getMessage('bigHeadEffect');
};

/**
 * @constructor
 * @extends {camera.effects.Pinch}
 */
camera.effects.BigJaw = function() {
  camera.effects.Pinch.call(this, 0.9, 0.6, 1.0);
};

camera.effects.BigJaw.prototype = {
  __proto__: camera.effects.Pinch.prototype
};

/**
 * @override
 */
camera.effects.BigJaw.prototype.getTitle = function() {
  return chrome.i18n.getMessage('bigJawEffect');
};

/**
 * @constructor
 * @extends {camera.effects.Pinch}
 */
camera.effects.BunnyHead = function() {
  camera.effects.Pinch.call(this, 0.8, 0.8, -0.7);
};

camera.effects.BunnyHead.prototype = {
  __proto__: camera.effects.Pinch.prototype
};

/**
 * @override
 */
camera.effects.BunnyHead.prototype.getTitle = function() {
  return chrome.i18n.getMessage('bunnyHeadEffect');
};

