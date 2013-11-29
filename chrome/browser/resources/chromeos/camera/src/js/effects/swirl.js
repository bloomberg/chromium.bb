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
camera.effects.Swirl = function(tracker) {
  camera.Effect.call(this, tracker);
  Object.freeze(this);
};

camera.effects.Swirl.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Swirl.prototype.filterFrame = function(canvas) {
  var face = this.tracker_.getFaceForCanvas(canvas);
  var x = canvas.width * (face.x + (face.width / 2));
  var y = canvas.height * face.y;
  var radius = Math.sqrt(Math.pow(face.width * canvas.width, 2) +
                         Math.pow(face.height * canvas.height, 2));
  canvas.swirl(x, y, radius, 1.5 * Math.sin(Date.now() / 300));
};

/**
 * @override
 */
camera.effects.Swirl.prototype.getTitle = function() {
  return chrome.i18n.getMessage('swirlEffect');
};

