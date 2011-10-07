// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The overlay displaying the image.
 */
function ImageView(container, viewport) {
  this.container_ = container;
  this.viewport_ = viewport;
  this.document_ = container.ownerDocument;
  this.contentGeneration_ = 0;
  this.displayedContentGeneration_ = 0;
  this.displayedViewportGeneration_ = 0;
}

ImageView.ANIMATION_DURATION = 180;
ImageView.ANIMATION_WAIT_INTERVAL = ImageView.ANIMATION_DURATION * 2;
ImageView.FAST_SCROLL_INTERVAL = 300;

ImageView.prototype = {__proto__: ImageBuffer.Overlay.prototype};

// Draw below overlays with the default zIndex.
ImageView.prototype.getZIndex = function() { return -1 };

ImageView.prototype.draw = function() {
  var forceRepaint = false;

  var screenClipped = this.viewport_.getScreenClipped();

  if (this.displayedViewportGeneration_ !=
      this.viewport_.getCacheGeneration()) {
    this.displayedViewportGeneration_ = this.viewport_.getCacheGeneration();

    if (this.screenCanvas_.width != screenClipped.width)
      this.screenCanvas_.width = screenClipped.width;

    if (this.screenCanvas_.height != screenClipped.height)
      this.screenCanvas_.height = screenClipped.height;

    this.screenCanvas_.style.left = screenClipped.left + 'px';
    this.screenCanvas_.style.top = screenClipped.top + 'px';

    forceRepaint = true;
  }

  if (forceRepaint ||
      this.displayedContentGeneration_ != this.contentGeneration_) {
    this.displayedContentGeneration_ = this.contentGeneration_;

    ImageUtil.trace.resetTimer('paint');
    this.paintScreenRect(
        screenClipped, this.contentCanvas_, this.viewport_.getImageClipped());
    ImageUtil.trace.reportTimer('paint');
  }
};

ImageView.prototype.getCursorStyle = function (x, y, mouseDown) {
  // Indicate that the image is draggable.
  if (this.viewport_.isClipped() &&
      this.viewport_.getScreenClipped().inside(x, y))
    return 'move';

  return null;
};

ImageView.prototype.getDragHandler = function (x, y) {
  var cursor = this.getCursorStyle(x, y);
  if (cursor == 'move') {
    // Return the handler that drags the entire image.
    return this.viewport_.createOffsetSetter(x, y);
  }

  return null;
};

ImageView.prototype.getCacheGeneration = function() {
  return this.contentGeneration_;
};

ImageView.prototype.invalidateCaches = function() {
  this.contentGeneration_++;
};

ImageView.prototype.getCanvas = function() { return this.contentCanvas_ };

ImageView.prototype.paintScreenRect = function (screenRect, canvas, imageRect) {
  // Map screen canvas (0,0) to (screenClipped.left, screenClipped.top)
  var screenClipped = this.viewport_.getScreenClipped();
  screenRect = screenRect.shift(-screenClipped.left, -screenClipped.top);

  // The source canvas may have different physical size than the image size
  // set at the viewport. Adjust imageRect accordingly.
  var bounds = this.viewport_.getImageBounds();
  var scaleX = canvas.width / bounds.width;
  var scaleY = canvas.height / bounds.height;
  imageRect = new Rect(imageRect.left * scaleX, imageRect.top * scaleY,
                       imageRect.width * scaleX, imageRect.height * scaleY);
  Rect.drawImage(
      this.screenCanvas_.getContext("2d"), canvas, screenRect, imageRect);
};

/**
 * @return {ImageData} A new ImageData object with a copy of the content.
 */
ImageView.prototype.copyScreenImageData = function () {
  return this.screenCanvas_.getContext("2d").getImageData(
      0, 0, this.screenCanvas_.width, this.screenCanvas_.height);
};

ImageView.prototype.isLoading = function() {
  return this.cancelThumbnailLoad_ || this.cancelImageLoad_;
};

ImageView.prototype.cancelLoad = function() {
  if (this.cancelThumbnailLoad_) {
    this.cancelThumbnailLoad_();
    this.cancelThumbnailLoad_ = null;
  }
  if (this.cancelImageLoad_) {
    this.cancelImageLoad_();
    this.cancelImageLoad_ = null;
  }
};

/**
 * Load and display a new image.
 *
 * Loads the thumbnail first, then replaces it with the main image.
 * Takes into account the image orientation encoded in the metadata.
 *
 * @param {string|HTMLCanvasElement|HTMLImageElement} source
 * @param {Object} metadata
 * @param {Object} slide Slide-in animation direction.
 * @param {function} opt_callback
 */
ImageView.prototype.load = function(
    source, metadata, slide, opt_callback) {

  metadata = metadata|| {};

  this.cancelLoad();

  ImageUtil.trace.resetTimer('load');
  var canvas = this.container_.ownerDocument.createElement('canvas');

  var self = this;

  if (metadata.thumbnailURL) {
    this.cancelImageLoad_ = ImageUtil.loadImageAsync(
        canvas,
        metadata.thumbnailURL,
        metadata.thumbnailTransform,
        0, /* no delay */
        displayThumbnail);
  } else {
    loadMainImage(0);
  }

  function displayThumbnail() {
    self.cancelThumbnailLoad_ = null;

    // The thumbnail may have different aspect ratio than the main image.
    // Force the main image proportions to avoid flicker.
    var time = Date.now();

    var mainImageLoadDelay = ImageView.ANIMATION_WAIT_INTERVAL;

    // Do not do slide-in animation when scrolling very fast.
    if (self.lastLoadTime_ &&
        (time - self.lastLoadTime_) < ImageView.FAST_SCROLL_INTERVAL) {
      slide = 0;
    }
    self.lastLoadTime_ = time;

    self.replace(canvas, slide, metadata.width, metadata.height);
    if (!slide) mainImageLoadDelay = 0;
    slide = 0;
    loadMainImage(mainImageLoadDelay);
  }

  function loadMainImage(delay) {
    self.cancelImageLoad_ = ImageUtil.loadImageAsync(
        canvas, source, metadata.imageTransform, delay, displayMainImage);
  }

  function displayMainImage() {
    self.cancelImageLoad_ = null;
    self.replace(canvas, slide);
    ImageUtil.trace.reportTimer('load');
    if (opt_callback) opt_callback();
  }
};

ImageView.prototype.replaceContent_ = function(
    canvas, opt_reuseScreenCanvas, opt_width, opt_height) {
  if (!opt_reuseScreenCanvas || !this.screenCanvas_) {
    this.screenCanvas_ = this.document_.createElement('canvas');
    this.screenCanvas_.style.webkitTransitionDuration =
        ImageView.ANIMATION_DURATION + 'ms';
  }

  this.contentCanvas_ = canvas;
  this.invalidateCaches();
  this.viewport_.setImageSize(
      opt_width || this.contentCanvas_.width,
      opt_height || this.contentCanvas_.height);
  this.viewport_.fitImage();
  this.viewport_.update();
  this.draw();

  if (opt_reuseScreenCanvas && !this.screenCanvas_.parentNode) {
    this.container_.appendChild(this.screenCanvas_);
  }
};

/**
 * Replace the displayed image, possibly with slide-in animation.
 *
 * @param {HTMLCanvasElement} canvas
 * @param {number} opt_slide Slide-in animation direction.
 *           <0 for right-to-left, > 0 for left-to-right, 0 for no animation.
 */
ImageView.prototype.replace = function(
    canvas, opt_slide, opt_width, opt_height) {
  var oldScreenCanvas = this.screenCanvas_;

  this.replaceContent_(canvas, !opt_slide, opt_width, opt_height);
  if (!opt_slide) return;

  var newScreenCanvas = this.screenCanvas_;

  function numToSlideAttr(num) {
    return num < 0 ? 'left' : num > 0 ? 'right' : 'center';
  }

  newScreenCanvas.setAttribute('fade', numToSlideAttr(opt_slide));
  this.container_.appendChild(newScreenCanvas);

  setTimeout(function() {
    newScreenCanvas.removeAttribute('fade');
    if (oldScreenCanvas) {
      oldScreenCanvas.setAttribute('fade', numToSlideAttr(-opt_slide));
      setTimeout(function() {
        oldScreenCanvas.parentNode.removeChild(oldScreenCanvas);
      }, ImageView.ANIMATION_WAIT_INTERVAL);
    }
  }, 0);
};

ImageView.makeTransform = function(rect1, rect2, scale, rotate90) {
  var shiftX = (rect1.left + rect1.width / 2) - (rect2.left + rect2.width / 2);
  var shiftY = (rect1.top + rect1.height / 2) - (rect2.top + rect2.height / 2);

  return 'rotate(' + (rotate90 || 0) * 90 + 'deg) ' +
      'translate(' + shiftX + 'px,' + shiftY + 'px)' +
      'scaleX(' + scale + ') ' +
      'scaleY(' + scale + ')';
};

/**
 * Hide the old image instantly, animate the new image to visualize
 * cropping and/or rotation.
 */
ImageView.prototype.replaceAndAnimate = function(canvas, cropRect, rotate90) {
  cropRect  = cropRect || this.viewport_.getScreenClipped();
  var oldScale = this.viewport_.getScale();

  var oldScreenCanvas = this.screenCanvas_;
  this.replaceContent_(canvas);
  var newScreenCanvas = this.screenCanvas_;

  // Display the new canvas, initially transformed.

  // Transform instantly.
  var duration = newScreenCanvas.style.webkitTransitionDuration;
  newScreenCanvas.style.webkitTransitionDuration = '0ms';

  newScreenCanvas.style.webkitTransform = ImageView.makeTransform(
      cropRect,
      this.viewport_.getScreenClipped(),
      oldScale / this.viewport_.getScale(),
      -rotate90);

  oldScreenCanvas.parentNode.appendChild(newScreenCanvas);
  oldScreenCanvas.parentNode.removeChild(oldScreenCanvas);

  // Let the layout fire.
  setTimeout(function() {
    // Animated back to non-transformed state.
    newScreenCanvas.style.webkitTransitionDuration = duration;
    newScreenCanvas.style.webkitTransform = '';
  }, 0);
};

/**
 * Shrink the given current image to the given crop rectangle while fading in
 * the new image.
 */
ImageView.prototype.animateAndReplace = function(canvas, cropRect) {
  var fullRect = this.viewport_.getScreenClipped();
  var oldScale = this.viewport_.getScale();

  var oldScreenCanvas = this.screenCanvas_;
  this.replaceContent_(canvas);
  var newCanvas = this.screenCanvas_;

  newCanvas.setAttribute('fade', 'center');
  oldScreenCanvas.parentNode.insertBefore(newCanvas, oldScreenCanvas);

  // Animate to the transformed state.

  oldScreenCanvas.style.webkitTransform = ImageView.makeTransform(
      cropRect,
      fullRect,
      this.viewport_.getScale() / oldScale,
      0);

  setTimeout(function() { newCanvas.removeAttribute('fade') }, 0);

  setTimeout(function() {
    oldScreenCanvas.parentNode.removeChild(oldScreenCanvas);
  }, ImageView.ANIMATION_WAIT_INTERVAL);
};
