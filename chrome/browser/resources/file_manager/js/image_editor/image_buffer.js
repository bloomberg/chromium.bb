// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The ImageBuffer object holds an offscreen canvas object and
 * draws its content on the screen canvas applying scale and offset.
 * Supports pluggable overlays that modify the image appearance and behavior.
 * @constructor
 */
function ImageBuffer(screenCanvas) {
  this.screenCanvas_ = screenCanvas;
  this.screenContext_ = this.screenCanvas_.getContext("2d");

  this.scale_ = 1;
  this.offsetX_ = 0;
  this.offsetY_ = 0;

  this.overlays_ = [];

  this.setImageCanvas(this.createBlankCanvas(0, 0));
}

/*
 * Viewport manipulation.
 */

ImageBuffer.prototype.setScaleControl = function(scaleControl) {
  this.scaleControl_ = scaleControl;
};

ImageBuffer.prototype.getScale = function () { return this.scale_ };

ImageBuffer.prototype.setScale = function (scale, notify) {
  if (this.scale_ == scale) return;
  this.scale_ = scale;
  if (notify && this.scaleControl_) this.scaleControl_.displayScale(scale);
};

ImageBuffer.prototype.getFittingScale = function() {
  var scaleX = this.screenCanvas_.width / this.imageCanvas_.width;
  var scaleY = this.screenCanvas_.height / this.imageCanvas_.height;
  return Math.min(scaleX, scaleY) * 0.85;
};

ImageBuffer.prototype.fitImage = function() {
  var scale = this.getFittingScale();
  if (this.scaleControl_) this.scaleControl_.setMinScale(scale);
  this.setScale(scale, true);
};

ImageBuffer.prototype.resizeScreen = function(width, height, keepFitting) {
  var wasFitting = this.getScale() == this.getFittingScale();

  this.screenCanvas_.width = width;
  this.screenCanvas_.height = height;

  var minScale = this.getFittingScale();
  if (this.scaleControl_) this.scaleControl_.setMinScale(minScale);
  if ((wasFitting && keepFitting) || this.getScale() < minScale) {
    this.setScale(minScale, true);
  }
  this.repaint();
};

ImageBuffer.prototype.getOffsetX = function () { return this.offsetX_; };

ImageBuffer.prototype.getOffsetY = function () { return this.offsetY_; };

ImageBuffer.prototype.setOffset = function(x, y, ignoreClipping) {
  if (!ignoreClipping) {
    var limitX = Math.max(0, -this.marginX_ / this.getScale());
    var limitY = Math.max(0, -this.marginY_ / this.getScale());
    x = ImageUtil.clip(-limitX, x, limitX);
    y = ImageUtil.clip(-limitY, y, limitY);
  }
  if (this.offsetX_ == x && this.offsetY_ == y) return;
  this.offsetX_ = x;
  this.offsetY_ = y;
};

ImageBuffer.prototype.setCenter = function(x, y, ignoreClipping) {
  this.setOffset(
      this.imageWhole_.width / 2 - x,
      this.imageWhole_.height / 2 - y,
      ignoreClipping);
};

/**
 * @return {Rect} The visible part of the image, in image coordinates.
 */
ImageBuffer.prototype.getClippedImage = function() {
  return this.imageVisible_;
};

/**
 * @return {Rect} The visible part of the image, in screen coordinates.
 */
ImageBuffer.prototype.getClippedScreen = function() {
  return this.screenVisible_;
};

/**
 * Returns a closure that can be called to pan the image.
 * Useful for implementing non-trivial variants of panning (overview etc).
 * @param {Number} originalX The x coordinate on the screen canvas that
 *                 corresponds to zero change to offsetX.
 * @param {Number} originalY The y coordinate on the screen canvas that
 *                 corresponds to zero change to offsetY.
 * @param {Function} scaleFunc returns the current image to screen scale.
 * @param {Function} hitFunc returns true if (x,y) is in the valid region.
 */
ImageBuffer.prototype.createOffsetSetter_ = function (
    originalX, originalY, scaleFunc, hitFunc) {
  var self = this;
  var originalOffsetX = this.offsetX_;
  var originalOffsetY = this.offsetY_;
  if (!hitFunc) hitFunc = function() { return true; }
  if (!scaleFunc) scaleFunc = this.getScale.bind(this);
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

/**
 * @return {Boolean} True if the entire image is visible on the screen canvas.
 */
ImageBuffer.prototype.isFullyVisible = function () {
  return this.marginX_ >= 0 && this.marginY_ >= 0;
};

ImageBuffer.prototype.updateViewPort = function () {
  var scale = this.getScale();

  this.screenWhole_ = new Rect(this.screenCanvas_);
  this.imageWhole_ = new Rect(this.imageCanvas_);

  // Image bounds in screen coordinates.
  this.imageOnScreen_ = {};

  this.imageOnScreen_.width = Math.floor(this.imageWhole_.width * scale);
  this.imageOnScreen_.height = Math.floor(this.imageWhole_.height * scale);

  this.marginX_ = Math.floor(
      (this.screenCanvas_.width  - this.imageOnScreen_.width) / 2);
  this.marginY_ = Math.floor(
      (this.screenCanvas_.height - this.imageOnScreen_.height) / 2);

  this.imageOnScreen_.left = this.marginX_;
  this.imageOnScreen_.top = this.marginY_;

  this.imageVisible_ = new Rect(this.imageWhole_);
  this.screenVisible_ = new Rect(this.screenWhole_);

  if (this.marginX_ < 0) {
    this.imageOnScreen_.left +=
        ImageUtil.clip(this.marginX_, this.offsetX_ * scale, -this.marginX_);
    this.imageVisible_.left = -this.imageOnScreen_.left / scale;
    this.imageVisible_.width = this.screenCanvas_.width / scale;
  } else {
    this.screenVisible_.left = this.imageOnScreen_.left;
    this.screenVisible_.width = this.imageOnScreen_.width;
  }

  if (this.marginY_ < 0) {
    this.imageOnScreen_.top +=
        ImageUtil.clip(this.marginY_, this.offsetY_ * scale, -this.marginY_);
    this.imageVisible_.top = -this.imageOnScreen_.top / scale;
    this.imageVisible_.height = this.screenCanvas_.height / scale;
  } else {
    this.screenVisible_.top = this.imageOnScreen_.top;
    this.screenVisible_.height = this.imageOnScreen_.height;
  }

  this.updateOverlays();
};

/*
 * Coordinate conversion between the screen canvas and the image.
 */

ImageBuffer.prototype.screenToImageSize = function(size) {
  return size / this.getScale();
};

ImageBuffer.prototype.screenToImageX = function(x) {
  return Math.round((x - this.imageOnScreen_.left) / this.getScale());
};

ImageBuffer.prototype.screenToImageY = function(y) {
  return Math.round((y - this.imageOnScreen_.top) / this.getScale());
};

ImageBuffer.prototype.screenToImageRect = function(rect) {
  return new Rect(
      this.screenToImageX(rect.left),
      this.screenToImageY(rect.top),
      this.screenToImageSize(rect.width),
      this.screenToImageSize(rect.height));
};

ImageBuffer.prototype.imageToScreenSize = function(size) {
  return size * this.getScale();
};

ImageBuffer.prototype.imageToScreenX = function(x) {
  return Math.round(this.imageOnScreen_.left + x * this.getScale());
};

ImageBuffer.prototype.imageToScreenY = function(y) {
  return Math.round(this.imageOnScreen_.top + y * this.getScale());
};

ImageBuffer.prototype.imageToScreenRect = function(rect) {
  return new Rect(
      this.imageToScreenX(rect.left),
      this.imageToScreenY(rect.top),
      this.imageToScreenSize(rect.width),
      this.imageToScreenSize(rect.height));
};

/*
 * Content manipulation.
 */

/**
 * Loads the new content.
 * A string parameter is treated as an image url.
 * @param {String|HTMLImageElement|HTMLCanvasElement} source
 */
ImageBuffer.prototype.load = function(source) {
  if (typeof source == 'string') {
    var self = this;
    var image = new Image();
    image.onload = function(e) { self.load(e.target); };
    image.src = source;
  } else {
    this.imageCanvas_.width = source.width,
    this.imageCanvas_.height = source.height;
    this.drawImage(source);

    if (this.scaleControl_)
      this.scaleControl_.displayImageSize(
          this.imageCanvas_.width, this.imageCanvas_.height);
    this.fitImage();
    this.repaint();
  }
};

ImageBuffer.prototype.getImageCanvas = function() { return this.imageCanvas_; };

/**
 * Replaces the off-screen canvas.
 * To be used when the editor modifies the image dimensions.
 * @param {HTMLCanvasElement} canvas
 */
ImageBuffer.prototype.setImageCanvas = function(canvas) {
  this.imageCanvas_ = canvas;
  this.imageContext_ = canvas.getContext("2d");
  if (this.scaleControl_)
    this.scaleControl_.displayImageSize(
        this.imageCanvas_.width, this.imageCanvas_.height);
};

/**
 * @return {HTMLCanvasElement} A new blank canvas of the required size.
 */
ImageBuffer.prototype.createBlankCanvas = function (width, height) {
  var canvas = this.screenCanvas_.ownerDocument.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  return canvas;
};

/**
 * @return {HTMLCanvasElement} A new canvas with a copy of the content.
 */
ImageBuffer.prototype.copyImageCanvas = function () {
  var canvas = this.createBlankCanvas(
      this.imageCanvas_.width, this.imageCanvas_.height);
  canvas.getContext('2d').drawImage(this.imageCanvas_, 0, 0);
  return canvas;
};

/**
 * @return {ImageData} A new ImageData object with a copy of the content.
 */
ImageBuffer.prototype.copyImageData = function () {
  return this.imageContext_.getImageData(
      0, 0, this.imageCanvas_.width, this.imageCanvas_.height);
};

/**
 * @param {HTMLImageElement|HTMLCanvasElement} image
 */
ImageBuffer.prototype.drawImage = function(image) {
  ImageUtil.trace.resetTimer('drawImage');
  this.imageContext_.drawImage(image, 0, 0);
  ImageUtil.trace.reportTimer('drawImage');
};

/**
 * @param {ImageData} imageData
 */
ImageBuffer.prototype.drawImageData = function (imageData) {
  ImageUtil.trace.resetTimer('putImageData');
  this.imageContext_.putImageData(imageData, 0, 0);
  ImageUtil.trace.reportTimer('putImageData');
};

/**
 * Paints the content on the screen canvas taking the current scale and offset
 * into account.
 */
ImageBuffer.prototype.repaint = function () {
  ImageUtil.trace.resetTimer('repaint');

  this.updateViewPort();

  this.screenContext_.save();

  this.screenContext_.fillStyle = '#F0F0F0';
  this.screenContext_.strokeStyle = '#000000';

  Rect.drawImage(this.screenContext_, this.imageCanvas_,
      this.imageOnScreen_, this.imageWhole_);
  Rect.fillBetween(this.screenContext_, this.imageOnScreen_,
      this.screenWhole_);
  Rect.stroke(this.screenContext_, this.imageOnScreen_);

  this.screenContext_.restore();

  this.drawOverlays(this.screenContext_);

  ImageUtil.trace.reportTimer('repaint');
};

/**
 * ImageBuffer.Overlay is a pluggable extension that modifies the outlook
 * and the behavior of the ImageBuffer instance.
 */
ImageBuffer.Overlay = function() {};

ImageBuffer.Overlay.prototype.getZIndex = function() { return 0 };

ImageBuffer.Overlay.prototype.updateViewPort = function() {}

ImageBuffer.Overlay.prototype.draw = function() {}

ImageBuffer.Overlay.prototype.getCursorStyle = function() { return null };

ImageBuffer.Overlay.prototype.onClick = function() { return false };

ImageBuffer.Overlay.prototype.getDragHandler = function() { return null };

/**
 * @param {ImageBuffer.Overlay} overlay
 */
ImageBuffer.prototype.addOverlay = function (overlay) {
  var zIndex = overlay.getZIndex();
  // Store the overlays in the ascending Z-order.
  var i;
  for (i = 0; i != this.overlays_.length; i++) {
    if (zIndex < this.overlays_[i].getZIndex()) break;
  }
  this.overlays_.splice(i, 0, overlay);
};

/**
 * @param {ImageBuffer.Overlay} overlay
 */
ImageBuffer.prototype.removeOverlay = function (overlay) {
  for (var i = 0; i != this.overlays_.length; i++) {
    if (this.overlays_[i] == overlay) {
      this.overlays_.splice(i, 1);
      return;
    }
  }
  throw new Error('Cannot remove overlay ' + overlay);
};

/**
 * Updates viewport configuration on all overlays.
 */
ImageBuffer.prototype.updateOverlays = function (context) {
  for (var i = 0; i != this.overlays_.length; i++) {
    this.overlays_[i].updateViewPort(this);
  }
}

/**
 * Draws overlays in the ascending Z-order.
 */
ImageBuffer.prototype.drawOverlays = function (context) {
  for (var i = 0; i != this.overlays_.length; i++) {
    context.save();
    this.overlays_[i].draw(context);
    context.restore();
  }
};

/**
 * Searches for a cursor style in the descending Z-order.
 * @return {String} A value for style.cursor CSS property.
 */
ImageBuffer.prototype.getCursorStyle = function (x, y, mouseDown) {
  for (var i = this.overlays_.length - 1; i >= 0; i--) {
    var style = this.overlays_[i].getCursorStyle(x, y, mouseDown);
    if (style) return style;
  }

  // Indicate that the image is draggable.
  if (!this.isFullyVisible() && this.screenVisible_.inside(x, y))
    return 'move';

  return 'default';
};

/**
 * Searches for a click handler in the descending Z-order.
 * @return {Boolean} True if handled.
 */
ImageBuffer.prototype.onClick = function (x, y) {
  for (var i = this.overlays_.length - 1; i >= 0; i--) {
    if (this.overlays_[i].onClick(x, y)) return true;
  }
  return false;
};

/**
 * Searches for a drag handler in the descending Z-order.
 * @return {Function} A closure to be called on mouse drag.
 */
ImageBuffer.prototype.getDragHandler = function (x, y) {
  for (var i = this.overlays_.length - 1; i >= 0; i--) {
    var handler = this.overlays_[i].getDragHandler(x, y);
    if (handler) return handler;
  }

  if (!this.isFullyVisible() && this.screenVisible_.inside(x, y)) {
    // Return the handler that drags the entire image.
    return this.createOffsetSetter_(x, y, this.getScale.bind(this));
  }

  return null;
};

/**
 * Overview overlay draws the image thumbnail in the bottom right corner.
 * Indicates the currently visible part.
 * Supports panning by dragging.
 */

ImageBuffer.Overview = function() {};

ImageBuffer.Overview.MAX_SIZE = 150;
ImageBuffer.Overview.RIGHT = 7;
ImageBuffer.Overview.BOTTOM = 50;

ImageBuffer.Overview.prototype = {__proto__: ImageBuffer.Overlay.prototype};

ImageBuffer.Overview.prototype.getZIndex = function() { return 100; }

ImageBuffer.Overview.prototype.updateViewPort = function(buffer) {
  this.buffer_ = buffer;

  this.whole_ = null;
  this.visible_ = null;

  if (this.buffer_.isFullyVisible()) return;

  var screenWhole = this.buffer_.screenWhole_;
  var imageWhole = this.buffer_.imageWhole_;
  var imageVisible = this.buffer_.imageVisible_;

  var aspect = imageWhole.width / imageWhole.height;
  if (aspect > 1) {
    this.whole_ = new Rect(ImageBuffer.Overview.MAX_SIZE,
                           ImageBuffer.Overview.MAX_SIZE / aspect);
  } else {
    this.whole_ = new Rect(ImageBuffer.Overview.MAX_SIZE * aspect,
                           ImageBuffer.Overview.MAX_SIZE);
  }

  this.whole_ = this.whole_.moveTo(
      screenWhole.width - ImageBuffer.Overview.RIGHT - this.whole_.width,
      screenWhole.height - ImageBuffer.Overview.BOTTOM - this.whole_.height);

  this.scale_ = this.whole_.width / imageWhole.width;

  this.visible_ = imageVisible.
      scale(this.scale_).
      shift(this.whole_.left, this.whole_.top);
};

ImageBuffer.Overview.prototype.draw = function(context) {
  if (!this.visible_) return;

  // Draw the thumbnail.
  Rect.drawImage(context, this.buffer_.imageCanvas_,
      this.whole_, this.buffer_.imageWhole_);

  // Draw the thumbnail border.
  context.strokeStyle = '#000000';
  Rect.stroke(context, this.whole_);

  // Draw the shadow over the off-screen part of the thumbnail.
  context.globalAlpha = 0.3;
  context.fillStyle = '#000000';
  Rect.fillBetween(context, this.visible_, this.whole_);

  // Outline the on-screen part of the thumbnail.
  context.strokeStyle = '#FFFFFF';
  Rect.stroke(context, this.visible_);
};

ImageBuffer.Overview.prototype.getCursorStyle = function(x, y) {
  if (!this.whole_ || !this.whole_.inside(x, y)) return null;

  // Indicate that the on-screen part is draggable.
  if (this.visible_.inside(x, y)) return 'move';

  // Indicathe that the rest of the thumbnail is clickable.
  return 'crosshair';
};

ImageBuffer.Overview.prototype.onClick = function(x, y) {
  if (!this.whole_ || !this.whole_.inside(x, y)) return false;

  if (this.visible_.inside(x, y)) return false;

  this.buffer_.setCenter(
      (x - this.whole_.left) / this.scale_,
      (y - this.whole_.top) / this.scale_);
  this.buffer_.repaint();
  return true;
};

ImageBuffer.Overview.prototype.getDragHandler = function(x, y) {
  if (!this.whole_ || !this.whole_.inside(x, y)) return null;

  if (this.visible_.inside(x, y)) {
    var self = this;
    function scale() { return -self.scale_;}
    function hit(x, y) { return self.whole_ && self.whole_.inside(x, y); }
    return this.buffer_.createOffsetSetter_(x, y, scale, hit);
  } else {
    // Force non-draggable behavior.
    return function() {};
  }
};
