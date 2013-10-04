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
 * @param {camera.Tracker} tracker Head tracker object.
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.Grayscale = function(tracker) {
  camera.Effect.call(this, tracker);
  Object.freeze(this);
};

camera.effects.Grayscale.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Grayscale.prototype.filterFrame = function(canvas) {
  canvas.hueSaturation(0, -1);
};

/**
 * @override
 */
camera.effects.Grayscale.prototype.getTitle = function() {
  return chrome.i18n.getMessage('grayscaleEffect');
};

