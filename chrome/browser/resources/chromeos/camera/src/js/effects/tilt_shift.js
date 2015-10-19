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
camera.effects.TiltShift = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.amount_ = 75;

  /**
   * @type {number}
   * @private
   */
  this.gradient_ = 2;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Returns true if the effect is available on the platform.
 *
 * @param {Array.<string>} webglExtensions Supported WebGL extensions on the
 *     platform.
 * @return {boolean} True if available, false otherwise.
 */
camera.effects.TiltShift.isAvailable = function(webglExtensions) {
  // Workaround for: crbug.com/523216.
  return (webglExtensions.indexOf('OES_texture_float_linear') !== -1);
};

camera.effects.TiltShift.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.TiltShift.prototype.randomize = function() {
  this.amount_ = Math.random() * 100 + 50;
  this.gradient_ = Math.random() * 4 + 1;
};

/**
 * @override
 */
camera.effects.TiltShift.prototype.filterFrame = function(canvas, faces) {
  canvas.tiltShift(0,
                   canvas.height * 0.4,
                   canvas.width - 1,
                   canvas.height * 0.4,
                   canvas.width / this.amount_,
                   canvas.height / this.gradient_);
};

/**
 * @override
 */
camera.effects.TiltShift.prototype.getTitle = function() {
  return chrome.i18n.getMessage('tiltShiftEffect');
};

/**
 * @override
 */
camera.effects.TiltShift.prototype.isSlow = function() {
  return true;
};

