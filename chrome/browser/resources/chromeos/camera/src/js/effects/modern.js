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
camera.effects.Modern = function() {
  camera.Effect.call(this);

  /**
   * @type {Array.<number>}
   * @private
   */
  this.colorIndexes_ = [0, 1, 2, 3];

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Available colors for parts of the image.
 * @type {Array.<Array<number>>}
 */
camera.effects.COLORS = Object.freeze([
  [0.5, 0, 0],
  [0, 0.5, 0],
  [0, 0, 0.5],
  [0.5, 0.5, 0],
  [0.5, 0, 0.5]
]);

camera.effects.Modern.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Modern.prototype.randomize = function() {
  for (var index = 0; index < 4; index++) {
    this.colorIndexes_[index] =
        Math.floor(Math.random() * camera.effects.COLORS.length);
  }
};

/**
 * @override
 */
camera.effects.Modern.prototype.filterFrame = function(canvas, faces) {
  canvas.modern(camera.effects.COLORS[this.colorIndexes_[0]],
                camera.effects.COLORS[this.colorIndexes_[1]],
                camera.effects.COLORS[this.colorIndexes_[2]],
                camera.effects.COLORS[this.colorIndexes_[3]]);
};

/**
 * @override
 */
camera.effects.Modern.prototype.getTitle = function() {
  return chrome.i18n.getMessage('modernEffect');
};

