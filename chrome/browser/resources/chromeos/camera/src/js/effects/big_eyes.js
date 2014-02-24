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
camera.effects.BigEyes = function() {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.amount_ = 0.3

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.BigEyes.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.randomize = function() {
  this.amount_ = Math.random() * 0.5 + 0.1;
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.filterFrame = function(canvas, faces) {
  for (var index = 0; index < faces.length; index++) {
    var x = canvas.width * (faces[index].x + (faces[index].width / 2));
    var y = canvas.height * faces[index].y;

    // Left eye.
    canvas.bulgePinch(x - (faces[index].width * canvas.width * -0.2),
                    y + (faces[index].height * canvas.height * 0.35),
                    faces[index].height * canvas.height * 0.3,
                    this.amount_);

    // Right eye.
    canvas.bulgePinch(x - (faces[index].width * canvas.width * 0.2),
                      y + (faces[index].height * canvas.height * 0.35),
                      faces[index].height * canvas.height * 0.3,
                      this.amount_);
  }
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.getTitle = function() {
  return chrome.i18n.getMessage('bigEyesEffect');
};

/**
 * @override
 */
camera.effects.BigEyes.prototype.usesHeadTracker = function() {
  return true;
};

