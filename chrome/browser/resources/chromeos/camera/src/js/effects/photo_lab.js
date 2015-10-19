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
camera.effects.PhotoLab = function() {
  camera.Effect.call(this);

  /**
   * @type {Array.<number>}
   * @private
   */
  this.minColor_ = [0.5, 0, 0];

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
camera.effects.PhotoLab.isAvailable = function(webglExtensions) {
  return true;
};

camera.effects.PhotoLab.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.PhotoLab.prototype.randomize = function() {
  var channelIndex = Math.floor(Math.random() * 3);
  this.minColor_ = [
    (channelIndex == 0 ? 0.2 : 0) + Math.random() * 0.5,
    (channelIndex == 1 ? 0.2 : 0) + Math.random() * 0.5,
    (channelIndex == 2 ? 0.2 : 0) + Math.random() * 0.5,
  ];
};

/**
 * @override
 */
camera.effects.PhotoLab.prototype.filterFrame = function(canvas, faces) {
  canvas.photolab(this.minColor_);
};

/**
 * @override
 */
camera.effects.PhotoLab.prototype.getTitle = function() {
  return chrome.i18n.getMessage('photoLabEffect');
};

