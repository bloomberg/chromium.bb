// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates a processor object, which takes the camera stream and processes it.
 * Flushes the result to a canvas.
 *
 * @param {Canvas|Video} input Canvas or Video with the input frame.
 * @param {fx.Canvas} output Canvas with the output frame.
 * @param {camera.Processor.Mode=} opt_mode Optional mode of the processor.
 *     Default is the high quality mode.
 * @constructor
 */
camera.Processor = function(input, output, opt_mode) {
  /**
   * @type {Canvas|Video}
   * @private
   */
  this.input_ = input;

  /**
   * @type {fx.Canvas}
   * @private
   */
  this.output_ = output;

  /**
   * @type {camera.Processor.Mode}
   * @private
   */
  this.mode_ = opt_mode || camera.Processor.Mode.DEFAULT;

  /**
   * @type {fx.Texture}
   * @private
   */
  this.texture_ = null;

  /**
   * @type {camera.Effect}
   * @private
   */
  this.effect_ = null;

  // End of properties. Seal the object.
  Object.seal(this);
};

/**
 * Type of the processor. For FAST it uses lowered resolution. DEFAULT is high
 * quality.
 * @enum {number}
 */
camera.Processor.Mode = Object.freeze({
  DEFAULT: 0,
  FAST: 1
});

camera.Processor.prototype = {
  set effect(inEffect) {
    this.effect_ = inEffect;
  },
  get effect() {
    return this.effect_;
  }
};

/**
 * Processes an input frame, applies effects and flushes result to the output
 * canvas.
 */
camera.Processor.prototype.processFrame = function() {
  var width = this.input_.videoWidth || this.input_.width;
  var height = this.input_.videoHeight || this.input_.height;
  if (!width || !height)
    return;

  if (!this.texture_)
    this.texture_ = this.output_.texture(this.input_);

  var textureWidth = null;
  var textureHeight = null;

  switch (this.mode_) {
    case camera.Processor.Mode.FAST:
      textureWidth = Math.round(width / 2);
      textureHeight = Math.round(height / 2);
      break;
    case camera.Processor.Mode.DEFAULT:
      textureWidth = width;
      textureHeight = height;
      break;
  }

  this.texture_.loadContentsOf(this.input_);
  this.output_.draw(this.texture_, textureWidth, textureHeight);
  if (this.effect_)
    this.effect_.filterFrame(this.output_);
  this.output_.update();
};

