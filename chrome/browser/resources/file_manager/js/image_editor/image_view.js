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

  this.imageLoader_ = new ImageUtil.ImageLoader(this.document_);
  // We have a separate image loader for prefetch which does not get cancelled
  // when the selection changes.
  this.prefetchLoader_ = new ImageUtil.ImageLoader(this.document_);

  // The content cache is used for prefetching the next image when going
  // through the images sequentially. The real life photos can be large
  // (18Mpix = 72Mb pixel array) so we want only the minimum amount of caching.
  this.contentCache_ = new ImageView.Cache(2);

  // We reuse previously generated screen-scale images so that going back to
  // a recently loaded image looks instant even if the image is not in
  // the content cache any more. Screen-scale images are small (~1Mpix)
  // so we can afford to cache more of them.
  this.screenCache_ = new ImageView.Cache(5);
  this.contentCallbacks_ = [];
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

ImageView.prototype.getThumbnail = function() { return this.thumbnailCanvas_ };

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
  return this.imageLoader_.isBusy();
};

ImageView.prototype.cancelLoad = function() {
  this.imageLoader_.cancel();
};

/**
 * Load and display a new image.
 *
 * Loads the thumbnail first, then replaces it with the main image.
 * Takes into account the image orientation encoded in the metadata.
 *
 * @param {number} id Unique image id for caching purposes
 * @param {string|HTMLCanvasElement} source
 * @param {Object} metadata
 * @param {Object} slide Slide-in animation direction.
 * @param {function(boolean} opt_callback The parameter is true if the image
 *    was loaded instantly (from the cache of the canvas source).
 */
ImageView.prototype.load = function(
    id, source, metadata, slide, opt_callback) {

  metadata = metadata|| {};

  ImageUtil.trace.resetTimer('load');

  var self = this;

  this.contentID_ = id;

  var readyContent = this.getReadyContent(id, source);
  if (readyContent) {
    displayMainImage(readyContent, true);
  } else {
    var cachedScreen = this.screenCache_.getItem(id);
    if (cachedScreen) {
      // We have a cached screen-scale canvas, use it as a thumbnail.
      displayThumbnail(cachedScreen);
    } else if (metadata.thumbnailURL) {
      this.imageLoader_.load(
          metadata.thumbnailURL,
          metadata.thumbnailTransform,
          displayThumbnail);
    } else {
      loadMainImage(0);
    }
  }

  function displayThumbnail(canvas) {
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
    if (self.prefetchLoader_.isLoading(source)) {
      // The image we need is already being prefetched. Initiating another load
      // would be a waste. Hijack the load instead by overriding the callback.
      self.prefetchLoader_.setCallback(displayMainImage);

      // Swap the loaders so that the self.isLoading works correctly.
      var temp = self.prefetchLoader_;
      self.prefetchLoader_ = self.imageLoader_;
      self.imageLoader_ = temp;
      return;
    }
    self.prefetchLoader_.cancel();  // The prefetch was doing something useless.

    self.imageLoader_.load(
        source,
        metadata.imageTransform,
        displayMainImage,
        delay);
  }

  function displayMainImage(canvas, opt_fastLoad) {
    self.replace(canvas, slide);
    ImageUtil.trace.reportTimer('load');
    ImageUtil.trace.report('load-type', opt_fastLoad ? 'cached' : 'file');
    if (opt_callback) opt_callback(opt_fastLoad);
  }
};

/**
 * Try to get the canvas from the content cache or from the source.
 *
 * @param {number} id Unique image id for caching purposes
 * @param {string|HTMLCanvasElement} source
 */
ImageView.prototype.getReadyContent = function(id, source) {
  if (source.constructor.name == 'HTMLCanvasElement')
    return source;

  return this.contentCache_.getItem(id);
};

/**
 * Prefetch an image.
 *
 * @param {number} id Unique image id for caching purposes
 * @param {string|HTMLCanvasElement} source
 * @param {Object} metadata
 */
ImageView.prototype.prefetch = function(id, source, metadata) {
  var self = this;
  function prefetchDone(canvas) {
    self.contentCache_.putItem(id, canvas);
  }

  var cached = this.getReadyContent(id, source);
  if (cached) {
    prefetchDone(cached);
  } else {
    // Evict the LRU item before we allocate the new canvas to avoid unneeded
    // strain on memory.
    this.contentCache_.evictLRU();

    this.prefetchLoader_.load(
        source,
        metadata.imageTransform,
        prefetchDone,
        ImageView.ANIMATION_WAIT_INTERVAL);
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

  // If this is not a thumbnail, cache the content and the screen-scale image.
  if (!opt_width && !opt_height) {
    this.contentCache_.putItem(this.contentID_, this.contentCanvas_, true);
    this.screenCache_.putItem(this.contentID_, this.screenCanvas_);

    // TODO(kaznacheev): It is better to pass screenCanvas_ as it is usually
    // much smaller than contentCanvas_ and still contains the entire image.
    // Once we implement zoom/pan we should pass contentCanvas_ instead.
    this.updateThumbnail_(this.screenCanvas_);

    for (var i = 0; i != this.contentCallbacks_.length; i++) {
      this.contentCallbacks_[i]();
    }
  }
};

ImageView.prototype.addContentCallback = function(callback) {
  this.contentCallbacks_.push(callback);
};

ImageView.prototype.updateThumbnail_ = function(canvas) {
  ImageUtil.trace.resetTimer('thumb');
  var pixelCount = 10000;
  var downScale =
      Math.max(1, Math.sqrt(canvas.width * canvas.height / pixelCount));

  this.thumbnailCanvas_ = canvas.ownerDocument.createElement('canvas');
  this.thumbnailCanvas_.width = Math.round(canvas.width / downScale);
  this.thumbnailCanvas_.height = Math.round(canvas.height / downScale);
  Rect.drawImage(this.thumbnailCanvas_.getContext('2d'), canvas);
  ImageUtil.trace.reportTimer('thumb');
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


ImageView.Cache = function(capacity) {
  this.capacity_ = capacity;
  this.map_ = {};
  this.order_ = [];
};

ImageView.Cache.prototype.getItem = function(id) { return this.map_[id] };

ImageView.Cache.prototype.putItem = function(id, item, opt_keepLRU) {
  var pos = this.order_.indexOf(id);

  if ((pos >= 0) != (id in this.map_))
    throw new Error('Inconsistent cache state');

  if (id in this.map_) {
    if (!opt_keepLRU) {
      // Move to the end (most recently used).
      this.order_.splice(pos, 1);
      this.order_.push(id);
    }
  } else {
    this.evictLRU();
    this.order_.push(id);
  }

  this.map_[id] = item;

  if (this.order_.length > this.capacity_)
    throw new Error('Exceeded cache capacity');
};

ImageView.Cache.prototype.evictLRU = function() {
  if (this.order_.length == this.capacity_) {
    var id = this.order_.shift();
    delete this.map_[id];
  }
};
