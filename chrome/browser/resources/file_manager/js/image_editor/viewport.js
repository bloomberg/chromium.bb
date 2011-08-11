// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Viewport class controls the way the image is displayed (scale, offset etc).
 */
function Viewport(repaintCallback) {
  this.repaintCallback_ = repaintCallback;

  this.imageBounds_ = new Rect();
  this.screenBounds_ = new Rect();

  this.scale_ = 1;
  this.offsetX_ = 0;
  this.offsetY_ = 0;

  this.generation_ = 0;

  this.scaleControl_ = null;

  this.update();
}

/*
 * Viewport modification.
 */

Viewport.prototype.setScaleControl = function(scaleControl) {
  this.scaleControl_ = scaleControl;
};

Viewport.prototype.setImageSize = function(width, height) {
  this.imageBounds_ = new Rect(width, height);
  if (this.scaleControl_) this.scaleControl_.displayImageSize(width, height);
  this.invalidateCaches();
};

Viewport.prototype.setScreenSize = function(width, height) {
  this.screenBounds_ = new Rect(width, height);
  if (this.scaleControl_)
    this.scaleControl_.setMinScale(this.getFittingScale());
  this.invalidateCaches();
};

Viewport.prototype.getScale = function() { return this.scale_ };

Viewport.prototype.setScale = function(scale, notify) {
  if (this.scale_ == scale) return;
  this.scale_ = scale;
  if (notify && this.scaleControl_) this.scaleControl_.displayScale(scale);
  this.invalidateCaches();
};

Viewport.prototype.getFittingScale = function() {
  var scaleX = this.screenBounds_.width / this.imageBounds_.width;
  var scaleY = this.screenBounds_.height / this.imageBounds_.height;
  return Math.min(scaleX, scaleY) * 0.85;
};

Viewport.prototype.fitImage = function() {
  var scale = this.getFittingScale();
  if (this.scaleControl_) this.scaleControl_.setMinScale(scale);
  this.setScale(scale, true);
};

Viewport.prototype.getOffsetX = function () { return this.offsetX_ };

Viewport.prototype.getOffsetY = function () { return this.offsetY_ };

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

Viewport.prototype.setCenter = function(x, y, ignoreClipping) {
  this.setOffset(
      this.imageBounds_.width / 2 - x,
      this.imageBounds_.height / 2 - y,
      ignoreClipping);
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
 */
Viewport.prototype.createOffsetSetter = function (
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

Viewport.prototype.screenToImageSize = function(size) {
  return size / this.getScale();
};

Viewport.prototype.screenToImageX = function(x) {
  return Math.round((x - this.imageOnScreen_.left) / this.getScale());
};

Viewport.prototype.screenToImageY = function(y) {
  return Math.round((y - this.imageOnScreen_.top) / this.getScale());
};

Viewport.prototype.screenToImageRect = function(rect) {
  return new Rect(
      this.screenToImageX(rect.left),
      this.screenToImageY(rect.top),
      this.screenToImageSize(rect.width),
      this.screenToImageSize(rect.height));
};

Viewport.prototype.imageToScreenSize = function(size) {
  return size * this.getScale();
};

Viewport.prototype.imageToScreenX = function(x) {
  return Math.round(this.imageOnScreen_.left + x * this.getScale());
};

Viewport.prototype.imageToScreenY = function(y) {
  return Math.round(this.imageOnScreen_.top + y * this.getScale());
};

Viewport.prototype.imageToScreenRect = function(rect) {
  return new Rect(
      this.imageToScreenX(rect.left),
      this.imageToScreenY(rect.top),
      this.imageToScreenSize(rect.width),
      this.imageToScreenSize(rect.height));
};

/**
 * @return {Boolean} True if some part of the image is clipped by the screen.
 */
Viewport.prototype.isClipped = function () {
  return this.getMarginX_() < 0 || this.getMarginY_() < 0;
};

/**
 * Horizontal margin. Negative if the image is clipped horizontally.
 */
Viewport.prototype.getMarginX_ = function() {
  return Math.round(
    (this.screenBounds_.width  - this.imageBounds_.width * this.scale_) / 2);
};

/**
 * Vertical margin. Negative if the image is clipped vertically.
 */
Viewport.prototype.getMarginY_ = function() {
  return Math.round(
    (this.screenBounds_.height - this.imageBounds_.height * this.scale_) / 2);
};

Viewport.prototype.clampOffsetX_ = function(x) {
  var limit = Math.round(Math.max(0, -this.getMarginX_() / this.getScale()));
  return ImageUtil.clamp(-limit, x, limit);
};

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

Viewport.prototype.repaint = function () {
  if (this.repaintCallback_) this.repaintCallback_();
};
