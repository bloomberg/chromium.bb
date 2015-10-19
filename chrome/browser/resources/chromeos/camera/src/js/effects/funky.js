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
camera.effects.Funky = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.amount_ = 6;

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
camera.effects.Funky.isAvailable = function(webglExtensions) {
  // Workaround for: crbug.com/523216.
  return (webglExtensions.indexOf('OES_texture_float_linear') !== -1);
};

camera.effects.Funky.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Funky.prototype.randomize = function() {
  this.amount_ = this.amount_ % 15 + 3;
};

/**
 * @override
 */
camera.effects.Funky.prototype.filterFrame = function(canvas, faces) {
  canvas.colorHalftone(320, 240, 0.25, this.amount_ / 720 * canvas.height);
};

/**
 * @override
 */
camera.effects.Funky.prototype.getTitle = function() {
  return chrome.i18n.getMessage('funkyEffect');
};

