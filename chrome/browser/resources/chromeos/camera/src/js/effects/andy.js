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
camera.effects.Andy = function(tracker) {
  camera.Effect.call(this, tracker);
  Object.freeze(this);
};

camera.effects.Andy.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Andy.prototype.filterFrame = function(canvas) {
  var faces = this.tracker_.getFacesForCanvas(canvas);
  for (var index = 0; index < faces.length; index++) {
    x = canvas.width * (faces[index].x + (faces[index].width / 2));
    y = canvas.height * faces[index].y;
    var radius = Math.sqrt(Math.pow(faces[index].width * canvas.width, 2) +
                           Math.pow(faces[index].height * canvas.height, 2));
    canvas.bulgePinch(x, y - radius, radius, -1);
  }
};

/**
 * @override
 */
camera.effects.Andy.prototype.getTitle = function() {
  return chrome.i18n.getMessage('andyEffect');
};

