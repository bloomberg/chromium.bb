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

  this.viewport_ = new Viewport(this.repaint.bind(this));
  this.viewport_.setScreenSize(screenCanvas.width, screenCanvas.height);

  this.content_ = new ImageBuffer.Content(
      this.viewport_, screenCanvas.ownerDocument);

  this.overlays_ = [];
  this.addOverlay(new ImageBuffer.Margin(this.viewport_));
  this.addOverlay(this.content_);
  // TODO(dgozman): consider adding overview in v2.
}

ImageBuffer.prototype.getViewport = function() { return this.viewport_ };

ImageBuffer.prototype.getContent = function() { return this.content_ };

/**
 * Loads the new content.
 * A string parameter is treated as an image url.
 * @param {String|HTMLImageElement|HTMLCanvasElement} source
 * @param {{scaleX: number, scaleY: number, rotate90: number}} opt_transform
 */
ImageBuffer.prototype.load = function(source, opt_transform) {
  if (typeof source == 'string') {
    var self = this;
    var image = new Image();
    image.onload = function(e) { self.load(e.target, opt_transform) };
    image.src = source;
  } else {
    this.content_.load(source, opt_transform);
    this.repaint();
  }
};

ImageBuffer.prototype.resizeScreen = function(width, height, keepFitting) {
  this.screenCanvas_.width = width;
  this.screenCanvas_.height = height;

  var wasFitting =
      this.viewport_.getScale() == this.viewport_.getFittingScale();

  this.viewport_.setScreenSize(width, height);

  var minScale = this.viewport_.getFittingScale();
  if ((wasFitting && keepFitting) || this.viewport_.getScale() < minScale) {
    this.viewport_.setScale(minScale, true);
  }
  this.repaint();
};

/**
 * Paints the content on the screen canvas taking the current scale and offset
 * into account.
 */
ImageBuffer.prototype.repaint = function (opt_fromOverlay) {
  this.viewport_.update();
  this.drawOverlays(this.screenCanvas_.getContext("2d"), opt_fromOverlay);
};

ImageBuffer.prototype.repaintScreenRect = function (screenRect, imageRect) {
  Rect.drawImage(
      this.screenCanvas_.getContext('2d'),
      this.getContent().getCanvas(),
      screenRect || this.getViewport().imageToScreenRect(screenRect),
      imageRect || this.getViewport().screenToImageRect(screenRect));
};

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
 * Draws overlays in the ascending Z-order.
 * Skips overlays below opt_startFrom.
 */
ImageBuffer.prototype.drawOverlays = function (context, opt_fromOverlay) {
  var skip = true;
  for (var i = 0; i != this.overlays_.length; i++) {
    var overlay = this.overlays_[i];
    if (!opt_fromOverlay || opt_fromOverlay == overlay) skip = false;
    if (skip) continue;

    context.save();
    overlay.draw(context);
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
 * @return {function(number,number)} A function to be called on mouse drag.
 */
ImageBuffer.prototype.getDragHandler = function (x, y) {
  for (var i = this.overlays_.length - 1; i >= 0; i--) {
    var handler = this.overlays_[i].getDragHandler(x, y);
    if (handler) return handler;
  }
  return null;
};

/**
 * ImageBuffer.Overlay is a pluggable extension that modifies the outlook
 * and the behavior of the ImageBuffer instance.
 */
ImageBuffer.Overlay = function() {};

ImageBuffer.Overlay.prototype.getZIndex = function() { return 0 };

ImageBuffer.Overlay.prototype.draw = function() {};

ImageBuffer.Overlay.prototype.getCursorStyle = function() { return null };

ImageBuffer.Overlay.prototype.onClick = function() { return false };

ImageBuffer.Overlay.prototype.getDragHandler = function() { return null };


/**
 * The margin overlay draws the image outline and paints the margins.
 */
ImageBuffer.Margin = function(viewport) {
  this.viewport_ = viewport;
};

ImageBuffer.Margin.prototype = {__proto__: ImageBuffer.Overlay.prototype};

// Draw below everything including the content.
ImageBuffer.Margin.prototype.getZIndex = function() { return -2 };

ImageBuffer.Margin.prototype.draw = function(context) {
  context.save();
  context.fillStyle = '#000000';
  context.strokeStyle = '#000000';
  Rect.fillBetween(context,
      this.viewport_.getImageBoundsOnScreen(),
      this.viewport_.getScreenBounds());

  Rect.outline(context, this.viewport_.getImageBoundsOnScreen());
  context.restore();
};

/**
 * The overlay containing the image.
 */
ImageBuffer.Content = function(viewport, document) {
  this.viewport_ = viewport;
  this.document_ = document;

  this.generation_ = 0;

  this.setCanvas(this.createBlankCanvas(0, 0));
};

ImageBuffer.Content.prototype = {__proto__: ImageBuffer.Overlay.prototype};

// Draw below overlays with the default zIndex.
ImageBuffer.Content.prototype.getZIndex = function() { return -1 };

ImageBuffer.Content.prototype.draw = function(context) {
  Rect.drawImage(
      context, this.canvas_, this.viewport_.getImageBoundsOnScreen());
};

ImageBuffer.Content.prototype.getCursorStyle = function (x, y, mouseDown) {
  // Indicate that the image is draggable.
  if (this.viewport_.isClipped() &&
      this.viewport_.getScreenClipped().inside(x, y))
    return 'move';

  return null;
};

ImageBuffer.Content.prototype.getDragHandler = function (x, y) {
  var cursor = this.getCursorStyle(x, y);
  if (cursor == 'move') {
    // Return the handler that drags the entire image.
    return this.viewport_.createOffsetSetter(x, y);
  }

  return null;
};

ImageBuffer.Content.prototype.getCacheGeneration = function() {
  return this.generation_;
};

ImageBuffer.Content.prototype.invalidateCaches = function() {
  this.generation_++;
};

ImageBuffer.Content.prototype.getCanvas = function() { return this.canvas_ };

ImageBuffer.Content.prototype.detachCanvas = function() {
  var canvas = this.canvas_;
  this.setCanvas(this.createBlankCanvas(0, 0));
  return canvas;
};

/**
 * Replaces the off-screen canvas.
 * To be used when the editor modifies the image dimensions.
 * If the logical width/height are supplied they override the canvas dimensions
 * and the canvas contents is scaled when displayed.
 * @param {HTMLCanvasElement} canvas
 * @param {number} opt_width Logical width (=canvas.width by default)
 * @param {number} opt_height Logical height (=canvas.height by default)
 */
ImageBuffer.Content.prototype.setCanvas = function(
    canvas, opt_width, opt_height) {
  this.canvas_ = canvas;
  this.viewport_.setImageSize(opt_width || canvas.width,
                              opt_height || canvas.height);

  this.invalidateCaches();
};

/**
 * @return {HTMLCanvasElement} A new blank canvas of the required size.
 */
ImageBuffer.Content.prototype.createBlankCanvas = function (width, height) {
  var canvas = this.document_.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  return canvas;
};

/**
 * @param {number} opt_width Width of the copy, original width by default.
 * @param {number} opt_height Height of the copy, original height by default.
 * @return {HTMLCanvasElement} A new canvas with a copy of the content.
 */
ImageBuffer.Content.prototype.copyCanvas = function (opt_width, opt_height) {
  var canvas = this.createBlankCanvas(opt_width || this.canvas_.width,
                                      opt_height || this.canvas_.height);
  Rect.drawImage(canvas.getContext('2d'), this.canvas_);
  return canvas;
};

/**
 * @return {ImageData} A new ImageData object with a copy of the content.
 */
ImageBuffer.Content.prototype.copyImageData = function (opt_width, opt_height) {
  return this.canvas_.getContext("2d").getImageData(
      0, 0, opt_width || this.canvas_.width, opt_height || this.canvas_.height);
};

/**
 * @param {HTMLImageElement|HTMLCanvasElement} image
 * @param {{scaleX: number, scaleY: number, rotate90: number}} opt_transform
 */
ImageBuffer.Content.prototype.load = function(image, opt_transform) {
  opt_transform = opt_transform || { scaleX: 1, scaleY: 1, rotate90: 0};

  if (opt_transform.rotate90 & 1) {  // Rotated +/-90deg, swap the dimensions.
    this.canvas_.width = image.height;
    this.canvas_.height = image.width;
  } else {
    this.canvas_.width = image.width;
    this.canvas_.height = image.height;
  }

  this.clear();
  ImageUtil.drawImageTransformed(
      this.canvas_,
      image,
      opt_transform.scaleX,
      opt_transform.scaleY,
      opt_transform.rotate90 * Math.PI / 2);
  this.invalidateCaches();

  this.viewport_.setImageSize(this.canvas_.width, this.canvas_.height);
  this.viewport_.fitImage();
};

ImageBuffer.Content.prototype.clear = function() {
  var context = this.canvas_.getContext("2d");
  context.globalAlpha = 1;
  context.fillStyle = '#FFFFFF';
  Rect.fill(context, new Rect(this.canvas_));
};

/**
 * @param {ImageData} imageData
 */
ImageBuffer.Content.prototype.drawImageData = function (imageData, x, y) {
  this.canvas_.getContext("2d").putImageData(imageData, x, y);
  this.invalidateCaches();
};

/**
 * The overview overlay draws the image thumbnail in the bottom right corner.
 * Indicates the currently visible part. Supports panning by dragging.
 */
ImageBuffer.Overview = function(viewport, content) {
  this.viewport_ = viewport;
  this.content_ = content;
  this.contentGeneration_ = 0;
};

ImageBuffer.Overview.prototype = {__proto__: ImageBuffer.Overlay.prototype};

// Draw above everything.
ImageBuffer.Overview.prototype.getZIndex = function() { return 100 };

ImageBuffer.Overview.MAX_SIZE = 150;
ImageBuffer.Overview.RIGHT = 7;
ImageBuffer.Overview.BOTTOM = 50;

ImageBuffer.Overview.prototype.update = function() {
  var imageBounds = this.viewport_.getImageBounds();

  if (this.contentGeneration_ != this.content_.getCacheGeneration()) {
    this.contentGeneration_ = this.content_.getCacheGeneration();

    var aspect = imageBounds.width / imageBounds.height;

    this.canvas_ = this.content_.copyCanvas(
        ImageBuffer.Overview.MAX_SIZE * Math.min(aspect, 1),
        ImageBuffer.Overview.MAX_SIZE / Math.max(aspect, 1));
  }

  this.bounds_ = null;
  this.clipped_ = null;

  if (this.viewport_.isClipped()) {
    var screenBounds = this.viewport_.getScreenBounds();

    this.bounds_ = new Rect(
        screenBounds.width - ImageBuffer.Overview.RIGHT - this.canvas_.width,
        screenBounds.height - ImageBuffer.Overview.BOTTOM - this.canvas_.height,
        this.canvas_.width,
        this.canvas_.height);

    this.scale_ = this.bounds_.width / imageBounds.width;

    this.clipped_ = this.viewport_.getImageClipped().
        scale(this.scale_).
        shift(this.bounds_.left, this.bounds_.top);
  }
};

ImageBuffer.Overview.prototype.draw = function(context) {
  this.update();

  if (!this.clipped_) return;

  // Draw the thumbnail.
  Rect.drawImage(context, this.canvas_, this.bounds_);

  // Draw the shadow over the off-screen part of the thumbnail.
  context.globalAlpha = 0.3;
  context.fillStyle = '#000000';
  Rect.fillBetween(context, this.clipped_, this.bounds_);

  // Outline the on-screen part of the thumbnail.
  context.strokeStyle = '#FFFFFF';
  Rect.outline(context, this.clipped_);

  context.globalAlpha = 1;
  // Draw the thumbnail border.
  context.strokeStyle = '#000000';
  Rect.outline(context, this.bounds_);
};

ImageBuffer.Overview.prototype.getCursorStyle = function(x, y) {
  if (!this.bounds_ || !this.bounds_.inside(x, y)) return null;

  // Indicate that the on-screen part is draggable.
  if (this.clipped_ && this.clipped_.inside(x, y)) return 'move';

  // Indicate that the rest of the thumbnail is clickable.
  return 'crosshair';
};

ImageBuffer.Overview.prototype.onClick = function(x, y) {
  if (this.getCursorStyle(x, y) != 'crosshair') return false;
  this.viewport_.setCenter(
      (x - this.bounds_.left) / this.scale_,
      (y - this.bounds_.top) / this.scale_);
  this.viewport_.repaint();
  return true;
};

ImageBuffer.Overview.prototype.getDragHandler = function(x, y) {
  var cursor = this.getCursorStyle(x, y);

  if (cursor == 'move') {
    var self = this;
    function scale() { return -self.scale_;}
    function hit(x, y) { return self.bounds_ && self.bounds_.inside(x, y); }
    return this.viewport_.createOffsetSetter(x, y, scale, hit);
  } else if (cursor == 'crosshair') {
    // Force non-draggable behavior.
    return function() {};
  } else {
    return null;
  }
};
