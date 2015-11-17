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
camera.effects.Ghost = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.brightness_ = 0.05;

  /**
   * @type {number}
   * @private
   */
  this.contrast_ = 0.2;

  // No more properties. Seal the object.
  Object.seal(this);
};

camera.effects.Ghost.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Ghost.prototype.randomize = function() {
  this.brightness_ = Math.random() * 0.4;
  this.contrast_ = Math.random() * 0.6 - 0.2;
};

/**
 * @override
 */
camera.effects.Ghost.prototype.filterFrame = function(canvas, faces) {
  canvas.ghost();
  canvas.hueSaturation(0, -1.0);
  canvas.brightnessContrast(this.brightness_, this.contrast_);
};

/**
 * @override
 */
camera.effects.Ghost.prototype.getTitle = function() {
  return chrome.i18n.getMessage('ghostEffect');
};

