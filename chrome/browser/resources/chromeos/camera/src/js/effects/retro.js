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
 * @param {number} rs Red channel shifting value. 0 for none, 1 for max.
 * @param {number} gs Green channel shifting value. 0 for none, 1 for max.
 * @param {number} bs Blue channel shifting value. 0 for none, 1 for max.
 * @param {number} rd Red channel dimming value. 0 for none, 1 for max.
 * @param {number} gd Green channel dimming value. 0 for none, 1 for max.
 * @param {number} bd Blue channel dimming value. 0 for none, 1 for max.
 * @constructor
 * @extends {camera.Effect}
 */
camera.effects.Retro = function(rs, gs, bs, rd, gd, bd) {
  camera.Effect.call(this);

  /**
   * @type {number}
   * @private
   */
  this.rs_ = rs;

  /**
   * @type {number}
   * @private
   */
  this.gs_ = gs;

  /**
   * @type {number}
   * @private
   */
  this.bs_ = bs;

  /**
   * @type {number}
   * @private
   */
  this.rd_ = rd;

  /**
   * @type {number}
   * @private
   */
  this.gd_ = gd;

  /**
   * @type {number}
   * @private
   */
  this.bd_ = bd;

  /**
   * @type {number}
   * @private
   */
  this.brightness_ = 0.1;

  /**
   * @type {number}
   * @private
   */
  this.contrast_ = -0.1;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.effects.Retro.prototype = {
  __proto__: camera.Effect.prototype
};

/**
 * @override
 */
camera.effects.Retro.prototype.randomize = function() {
  this.brightness_ = Math.random() * 0.6;
  this.contrast_ = Math.random() * -0.2;
};

/**
 * @override
 */
camera.effects.Retro.prototype.filterFrame = function(canvas, faces) {
  canvas.brightnessContrast(this.brightness_ / 2, this.contrast_ / 2);
  canvas.curves([[0, this.rs_], [1, 1 - this.rd_]],
                [[0, this.gs_], [1, 1 - this.gd_]],
                [[0, this.bs_], [1, 1 - this.bd_]]);
  canvas.brightnessContrast(this.brightness_ / 2, this.contrast_ / 2);
  canvas.vignette(0.5, 0.4);
};

/**
 * @constructor
 * @extends {camera.effects.Retro}
 */
camera.effects.Retro30 = function() {
  camera.effects.Retro.call(this, 0.1, 0, 0, 0, 0.1, 0.4);
};

camera.effects.Retro30.prototype = {
  __proto__: camera.effects.Retro.prototype
};

/**
 * @override
 */
camera.effects.Retro30.prototype.getTitle = function() {
  return chrome.i18n.getMessage('retro30Effect');
};

/**
 * @constructor
 * @extends {camera.effects.Retro}
 */
camera.effects.Retro50 = function() {
  camera.effects.Retro.call(this, 0.2, 0, 0, 0.2, 0.2, 0);
};

camera.effects.Retro50.prototype = {
  __proto__: camera.effects.Retro.prototype
};

/**
 * @override
 */
camera.effects.Retro50.prototype.getTitle = function() {
  return chrome.i18n.getMessage('retro50Effect');
};

/**
 * @constructor
 * @extends {camera.effects.Retro}
 */
camera.effects.Retro60 = function() {
  camera.effects.Retro.call(this, 0, 0, 0.2, 0.2, 0, 0);
};

camera.effects.Retro60.prototype = {
  __proto__: camera.effects.Retro.prototype
};

/**
 * @override
 */
camera.effects.Retro60.prototype.getTitle = function() {
  return chrome.i18n.getMessage('retro60Effect');
};

