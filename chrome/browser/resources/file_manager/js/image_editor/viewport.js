// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Viewport class controls the way the image is displayed (scale, offset etc).
 * @constructor
 */
function Viewport() {
  this.imageBounds_ = new Rect();
  this.screenBounds_ = new Rect();

  this.scale_ = 1;
  this.offsetX_ = 0;
  this.offsetY_ = 0;

  this.generation_ = 0;

  this.scaleControl_ = null;
  this.repaintCallbacks_ = [];
  this.update();
}

/*
 * Viewport modification.
 */

/**
 * @param {object} scaleControl The UI object responsible for scaling.
 */
Viewport.prototype.setScaleControl = function(scaleControl) {
  this.scaleControl_ = scaleControl;
};

/**
 * @param {number} width Image width.
 * @param {number} height Image height.
 */
Viewport.prototype.setImageSize = function(width, height) {
  this.imageBounds_ = new Rect(width, height);
  if (this.scaleControl_) this.scaleControl_.displayImageSize(width, height);
  this.invalidateCaches();
};

/**
 * @param {number} width Screen width.
 * @param {number} height Screen height.
 */
Viewport.prototype.setScreenSize = function(width, height) {
  this.screenBounds_ = new Rect(width, height);
  if (this.scaleControl_)
    this.scaleControl_.setMinScale(this.getFittingScale());
  this.invalidateCaches();
};

/**
 * Set the size by an HTML element.
 *
 * @param {HTMLElement} frame The element acting as the "screen".
 */
Viewport.prototype.sizeByFrame = function(frame) {
  this.setScreenSize(frame.clientWidth, frame.clientHeight);
};

/**
 * Set the size and scale to fit an HTML element.
 *
 * @param {HTMLElement} frame The element acting as the "screen".
 */
Viewport.prototype.sizeByFrameAndFit = function(frame) {
  var wasFitting = this.getScale() == this.getFittingScale();
  this.sizeByFrame(frame);
  var minScale = this.getFittingScale();
  if (wasFitting || (this.getScale() < minScale)) {
    this.setScale(minScale, true);
  }
};

/**
 * @return {number} Scale
 */
Viewport.prototype.getScale = function() { return this.scale_ };

/**
 * @param {number} scale The new scale.
 * @param {boolean} notify True if the change should be reflected in the UI.
 */
Viewport.prototype.setScale = function(scale, notify) {
  if (this.scale_ == scale) return;
  this.scale_ = scale;
  if (notify && this.scaleControl_) this.scaleControl_.displayScale(scale);
  this.invalidateCaches();
};

/**
 * @return {number} Best scale to fit the current image into the current screen.
 */
Viewport.prototype.getFittingScale = function() {
  var scaleX = this.screenBounds_.width / this.imageBounds_.width;
  var scaleY = this.screenBounds_.height / this.imageBounds_.height;
  // Scales > (1 / this.getDevicePixelRatio()) do not look good. Also they are
  // not really useful as we do not have any pixel-level operations.
  return Math.min(1 / Viewport.getDevicePixelRatio(), scaleX, scaleY);
};

/**
 * Set the scale to fit the image into the screen.
 */
Viewport.prototype.fitImage = function() {
  var scale = this.getFittingScale();
  if (this.scaleControl_) this.scaleControl_.setMinScale(scale);
  this.setScale(scale, true);
};

/**
 * @return {number} X-offset of the viewport.
 */
Viewport.prototype.getOffsetX = function() { return this.offsetX_ };

/**
 * @return {number} Y-offset of the viewport.
 */
Viewport.prototype.getOffsetY = function() { return this.offsetY_ };

/**
 * Set the image offset in the viewport.
 * @param {number} x X-offset.
 * @param {number} y Y-offset.
 * @param {boolean} ignoreClipping True if no clipping should be applied.
 */
Viewport.prototype.setOffset = function(x, y, ignoreClipping) {
  if (!ignoreClipping) {
    x = this.clampOffsetX_(x);
    y = this.clampOffsetY_(y);
  }
  if (this.offsetX_ == x && this.offsetY_ == y) return;
  this.offsetX_ = x;
  this.offsetY_ = y;
  this.invalidateCaches();
};

/**
 * Return a closure that can be called to pan the image.
 * Useful for implementing non-trivial variants of panning (overview etc).
 * @param {number} originalX The x coordinate on the screen canvas that
 *                 corresponds to zero change to offsetX.
 * @param {number} originalY The y coordinate on the screen canvas that
 *                 corresponds to zero change to offsetY.
 * @param {function():number} scaleFunc returns the image to screen scale.
 * @param {function(number,number):boolean} hitFunc returns true if (x,y) is
 *                                                  in the valid region.
 * @return {function} The closure to pan the image.
 */
Viewport.prototype.createOffsetSetter = function(
    originalX, originalY, scaleFunc, hitFunc) {
  var originalOffsetX = this.offsetX_;
  var originalOffsetY = this.offsetY_;
  if (!hitFunc) hitFunc = function() { return true };
  if (!scaleFunc) scaleFunc = this.getScale.bind(this);

  var self = this;
  return function(x, y) {
    if (hitFunc(x, y)) {
      var scale = scaleFunc();
      self.setOffset(
          originalOffsetX + (x - originalX) / scale,
          originalOffsetY + (y - originalY) / scale);
      self.repaint();
    }
  };
};

/*
 * Access to the current viewport state.
 */

/**
 * @return {Rect} The image bounds in image coordinates.
 */
Viewport.prototype.getImageBounds = function() { return this.imageBounds_ };

/**
* @return {Rect} The screen bounds in screen coordinates.
*/
Viewport.prototype.getScreenBounds = function() { return this.screenBounds_ };

/**
 * @return {Rect} The visible part of the image, in image coordinates.
 */
Viewport.prototype.getImageClipped = function() { return this.imageClipped_ };

/**
 * @return {Rect} The visible part of the image, in screen coordinates.
 */
Viewport.prototype.getScreenClipped = function() { return this.screenClipped_ };

/**
 * A counter that is incremented with each viewport state change.
 * Clients that cache anything that depends on the viewport state should keep
 * track of this counter.
 * @return {number} counter
 */
Viewport.prototype.getCacheGeneration = function() { return this.generation_ };

/**
 * Called on evert view port state change (even if repaint has not been called).
 */
Viewport.prototype.invalidateCaches = function() { this.generation_++ };

/**
 * @return {Rect} The image bounds in screen coordinates.
 */
Viewport.prototype.getImageBoundsOnScreen = function() {
  return this.imageOnScreen_;
};

/*
 * Conversion between the screen and image coordinate spaces.
 */

/**
 * @param {number} size Size in screen coordinates.
 * @return {number} Size in image coordinates.
 */
Viewport.prototype.screenToImageSize = function(size) {
  return size / this.getScale();
};

/**
 * @param {number} x X in screen coordinates.
 * @return {number} X in image coordinates.
 */
Viewport.prototype.screenToImageX = function(x) {
  return Math.round((x - this.imageOnScreen_.left) / this.getScale());
};

/**
 * @param {number} y Y in screen coordinates.
 * @return {number} Y in image coordinates.
 */
Viewport.prototype.screenToImageY = function(y) {
  return Math.round((y - this.imageOnScreen_.top) / this.getScale());
};

/**
 * @param {Rect} rect Rectange in screen coordinates.
 * @return {Rect} Rectange in image coordinates.
 */
Viewport.prototype.screenToImageRect = function(rect) {
  return new Rect(
      this.screenToImageX(rect.left),
      this.screenToImageY(rect.top),
      this.screenToImageSize(rect.width),
      this.screenToImageSize(rect.height));
};

/**
 * @param {number} size Size in image coordinates.
 * @return {number} Size in screen coordinates.
 */
Viewport.prototype.imageToScreenSize = function(size) {
  return size * this.getScale();
};

/**
 * @param {number} x X in image coordinates.
 * @return {number} X in screen coordinates.
 */
Viewport.prototype.imageToScreenX = function(x) {
  return Math.round(this.imageOnScreen_.left + x * this.getScale());
};

/**
 * @param {number} y Y in image coordinates.
 * @return {number} Y in screen coordinates.
 */
Viewport.prototype.imageToScreenY = function(y) {
  return Math.round(this.imageOnScreen_.top + y * this.getScale());
};

/**
 * @param {Rect} rect Rectange in image coordinates.
 * @return {Rect} Rectange in screen coordinates.
 */
Viewport.prototype.imageToScreenRect = function(rect) {
  return new Rect(
      this.imageToScreenX(rect.left),
      this.imageToScreenY(rect.top),
      Math.round(this.imageToScreenSize(rect.width)),
      Math.round(this.imageToScreenSize(rect.height)));
};

/**
 * @return {number} The number of physical pixels in one CSS pixel.
 */
Viewport.getDevicePixelRatio = function() { return window.devicePixelRatio };

/**
 * Convert a rectangle from screen coordinates to 'device' coordinates.
 *
 * This conversion enlarges the original rectangle devicePixelRatio times
 * with the screen center as a fixed point.
 *
 * @param {Rect} rect Rectangle in screen coordinates.
 * @return {Rect} Rectangle in device coordinates.
 */
Viewport.prototype.screenToDeviceRect = function(rect) {
  var ratio = Viewport.getDevicePixelRatio();
  var screenCenterX = Math.round(
      this.screenBounds_.left + this.screenBounds_.width / 2);
  var screenCenterY = Math.round(
      this.screenBounds_.top + this.screenBounds_.height / 2);
  return new Rect(screenCenterX + (rect.left - screenCenterX) * ratio,
                  screenCenterY + (rect.top - screenCenterY) * ratio,
                  rect.width * ratio,
                  rect.height * ratio);
};

/**
 * @return {Rect} The visible part of the image, in device coordinates.
 */
Viewport.prototype.getDeviceClipped = function() {
  return this.screenToDeviceRect(this.getScreenClipped());
};

/**
 * @return {boolean} True if some part of the image is clipped by the screen.
 */
Viewport.prototype.isClipped = function() {
  return this.getMarginX_() < 0 || this.getMarginY_() < 0;
};

/**
 * @return {number} Horizontal margin.
 *   Negative if the image is clipped horizontally.
 * @private
 */
Viewport.prototype.getMarginX_ = function() {
  return Math.round(
    (this.screenBounds_.width - this.imageBounds_.width * this.scale_) / 2);
};

/**
 * @return {number} Vertical margin.
 *   Negative if the image is clipped vertically.
 * @private
 */
Viewport.prototype.getMarginY_ = function() {
  return Math.round(
    (this.screenBounds_.height - this.imageBounds_.height * this.scale_) / 2);
};

/**
 * @param {number} x X-offset.
 * @return {number} X-offset clamped to the valid range.
 * @private
 */
Viewport.prototype.clampOffsetX_ = function(x) {
  var limit = Math.round(Math.max(0, -this.getMarginX_() / this.getScale()));
  return ImageUtil.clamp(-limit, x, limit);
};

/**
 * @param {number} y Y-offset.
 * @return {number} Y-offset clamped to the valid range.
 * @private
 */
Viewport.prototype.clampOffsetY_ = function(y) {
  var limit = Math.round(Math.max(0, -this.getMarginY_() / this.getScale()));
  return ImageUtil.clamp(-limit, y, limit);
};

/**
 * Recalculate the viewport parameters.
 */
Viewport.prototype.update = function() {
  var scale = this.getScale();

  // Image bounds in screen coordinates.
  this.imageOnScreen_ = new Rect(
      this.getMarginX_(),
      this.getMarginY_(),
      Math.round(this.imageBounds_.width * scale),
      Math.round(this.imageBounds_.height * scale));

  // A visible part of the image in image coordinates.
  this.imageClipped_ = new Rect(this.imageBounds_);

  // A visible part of the image in screen coordinates.
  this.screenClipped_ = new Rect(this.screenBounds_);

  // Adjust for the offset.
  if (this.imageOnScreen_.left < 0) {
    this.imageOnScreen_.left +=
        Math.round(this.clampOffsetX_(this.offsetX_) * scale);
    this.imageClipped_.left = Math.round(-this.imageOnScreen_.left / scale);
    this.imageClipped_.width = Math.round(this.screenBounds_.width / scale);
  } else {
    this.screenClipped_.left = this.imageOnScreen_.left;
    this.screenClipped_.width = this.imageOnScreen_.width;
  }

  if (this.imageOnScreen_.top < 0) {
    this.imageOnScreen_.top +=
        Math.round(this.clampOffsetY_(this.offsetY_) * scale);
    this.imageClipped_.top = Math.round(-this.imageOnScreen_.top / scale);
    this.imageClipped_.height = Math.round(this.screenBounds_.height / scale);
  } else {
    this.screenClipped_.top = this.imageOnScreen_.top;
    this.screenClipped_.height = this.imageOnScreen_.height;
  }
};

/**
 * @param {function} callback Repaint callback.
 */
Viewport.prototype.addRepaintCallback = function(callback) {
  this.repaintCallbacks_.push(callback);
};

/**
 * Repaint all clients.
 */
Viewport.prototype.repaint = function() {
  this.update();
  for (var i = 0; i != this.repaintCallbacks_.length; i++)
    this.repaintCallbacks_[i]();
};
