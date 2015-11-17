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
camera.effects.Vintage = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.brightness_ = 0;

  /**
   * @type {number}
   * @private
   */
  this.vibrance_ = -1;

  /**
   * @type {number}
   * @private
   */
  this.contrast_ = 0;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.Vintage.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Vintage.prototype.randomize = function() {
  this.brightness_ = Math.random() * 0.4;
  this.vibrance_ = Math.random() > 0.5 ? -1 : 1;
  this.contrast_ = Math.random() * 0.6 - 0.2;
};

/**
 * @override
 */
camera.effects.Vintage.prototype.filterFrame = function(canvas, faces) {
 canvas.brightnessContrast(this.brightness_, this.contrast_).
     vibrance(this.vibrance_).
     vignette(0.5, 0.4);
};

/**
 * @override
 */
camera.effects.Vintage.prototype.getTitle = function() {
  return chrome.i18n.getMessage('vintageEffect');
};

