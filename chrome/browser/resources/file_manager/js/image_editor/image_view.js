// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The overlay displaying the image.
 */
function ImageView(container, viewport, metadataProvider) {
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

  /**
   * The element displaying the current content.
   * @type {HTMLCanvasElement|HTMLVideoElement}
   */
  this.screenImage_ = null;

  this.localImageTransformFetcher_ = function(url, callback) {
    metadataProvider.fetchLocal(url, function(metadata) {
      callback(metadata.imageTransform);
    });
  };
}

ImageView.ANIMATION_DURATION = 180;
ImageView.ANIMATION_WAIT_INTERVAL = ImageView.ANIMATION_DURATION * 2;
ImageView.FAST_SCROLL_INTERVAL = 300;

ImageView.LOAD_TYPE_CACHED_FULL = 0;
ImageView.LOAD_TYPE_CACHED_SCREEN = 1;
ImageView.LOAD_TYPE_IMAGE_FILE = 2;
ImageView.LOAD_TYPE_VIDEO_FILE = 3;
ImageView.LOAD_TYPE_ERROR = 4;
ImageView.LOAD_TYPE_TOTAL = 5;

ImageView.prototype = {__proto__: ImageBuffer.Overlay.prototype};

// Draw below overlays with the default zIndex.
ImageView.prototype.getZIndex = function() { return -1 };

ImageView.prototype.draw = function() {
  if (!this.contentCanvas_)  // Do nothing if the image content is not set.
    return;

  var forceRepaint = false;

  if (this.displayedViewportGeneration_ !=
      this.viewport_.getCacheGeneration()) {
    this.displayedViewportGeneration_ = this.viewport_.getCacheGeneration();

    this.setupDeviceBuffer(this.screenImage_);

    forceRepaint = true;
  }

  if (forceRepaint ||
      this.displayedContentGeneration_ != this.contentGeneration_) {
    this.displayedContentGeneration_ = this.contentGeneration_;

    ImageUtil.trace.resetTimer('paint');
    this.paintDeviceRect(this.viewport_.getDeviceClipped(),
        this.contentCanvas_, this.viewport_.getImageClipped());
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

ImageView.prototype.hasValidImage = function() {
  return !this.preview_ && this.contentCanvas_ && this.contentCanvas_.width;
};

ImageView.prototype.getVideo = function() { return this.videoElement_ };

ImageView.prototype.getThumbnail = function() { return this.thumbnailCanvas_ };

ImageView.prototype.getContentRevision = function() {
  return this.contentRevision_;
};

/**
 * Copy an image fragment from a full resolution canvas to a device resolution
 * canvas.
 *
 * @param {Rect} deviceRect Rectangle in the device coordinates.
 * @param {HTMLCanvasElement} canvas Full resolution canvas.
 * @param {Rect} imageRect Rectangle in the full resolution canvas.
 */
ImageView.prototype.paintDeviceRect = function (deviceRect, canvas, imageRect) {
  // Map screen canvas (0,0) to (deviceBounds.left, deviceBounds.top)
  var deviceBounds = this.viewport_.getDeviceClipped();
  deviceRect = deviceRect.shift(-deviceBounds.left, -deviceBounds.top);

  // The source canvas may have different physical size than the image size
  // set at the viewport. Adjust imageRect accordingly.
  var bounds = this.viewport_.getImageBounds();
  var scaleX = canvas.width / bounds.width;
  var scaleY = canvas.height / bounds.height;
  imageRect = new Rect(imageRect.left * scaleX, imageRect.top * scaleY,
                       imageRect.width * scaleX, imageRect.height * scaleY);
  Rect.drawImage(
      this.screenImage_.getContext("2d"), canvas, deviceRect, imageRect);
};

/**
 * Create an overlay canvas with properties similar to the screen canvas.
 * Useful for showing quick feedback when editing.
 *
 * @return {HTMLCanvasElement} Overlay canvas
 */
ImageView.prototype.createOverlayCanvas = function() {
  var canvas = this.document_.createElement('canvas');
  canvas.className = 'image';
  this.container_.appendChild(canvas);
  return canvas;
};

/**
 * Sets up the canvas as a buffer in the device resolution.
 *
 * @param {HTMLCanvasElement} canvas The buffer canvas.
 */
ImageView.prototype.setupDeviceBuffer = function(canvas) {
  var deviceRect = this.viewport_.getDeviceClipped();

  // Set the canvas position and size in device pixels.
  if (canvas.width != deviceRect.width)
    canvas.width = deviceRect.width;

  if (canvas.height != deviceRect.height)
    canvas.height = deviceRect.height;

  canvas.style.left = deviceRect.left + 'px';
  canvas.style.top = deviceRect.top + 'px';

  // Scale the canvas down down to screen pixels.
  this.setTransform(canvas);
};

/**
 * @return {ImageData} A new ImageData object with a copy of the content.
 */
ImageView.prototype.copyScreenImageData = function () {
  return this.screenImage_.getContext("2d").getImageData(
      0, 0, this.screenImage_.width, this.screenImage_.height);
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
 * @param {function(number} opt_callback The parameter is the load type.
 */
ImageView.prototype.load = function(
    id, source, metadata, slide, opt_callback) {

  metadata = metadata|| {};

  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('DisplayTime'));

  var self = this;

  this.contentID_ = id;
  this.contentRevision_ = -1;

  var loadingVideo = FileType.getMediaType(source) == 'video';
  if (loadingVideo) {
    var video = this.document_.createElement('video');
    if (metadata.thumbnailURL) {
      video.setAttribute('poster', metadata.thumbnailURL);
      this.replace(video, slide); // Show the poster immediately.
    }
    video.addEventListener('loadedmetadata', onVideoLoad);
    video.addEventListener('error', onVideoLoad);

    // Do not try no stream when offline.
    video.src = (navigator.onLine && metadata.streamingURL) || source;
    video.load();

    function onVideoLoad() {
      video.removeEventListener('loadedmetadata', onVideoLoad);
      video.removeEventListener('error', onVideoLoad);
      displayMainImage(ImageView.LOAD_TYPE_VIDEO_FILE, slide,
          !!metadata.thumbnailURL /* preview shown */, video);
    }
    return;
  }
  var readyContent = this.getReadyContent(id, source);
  if (readyContent) {
    displayMainImage(ImageView.LOAD_TYPE_CACHED_FULL, slide,
        false /* no preview */, readyContent);
  } else {
    var cachedScreen = this.screenCache_.getItem(id);
    if (cachedScreen) {
      // We have a cached screen-scale canvas, use it instead of a thumbnail.
      displayThumbnail(ImageView.LOAD_TYPE_CACHED_SCREEN, slide, cachedScreen);
      // As far as the user can tell the image is loaded. We still need to load
      // the full res image to make editing possible, but we can report now.
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('DisplayTime'));
    } else if (metadata.thumbnailURL) {
      this.imageLoader_.load(
          metadata.thumbnailURL,
          function(url, callback) { callback(metadata.thumbnailTransform); },
          displayThumbnail.bind(null, ImageView.LOAD_TYPE_IMAGE_FILE, slide));
    } else {
      loadMainImage(ImageView.LOAD_TYPE_IMAGE_FILE, slide, source,
          false /* no preview*/, 0 /* delay */);
    }
  }

  function displayThumbnail(loadType, slide, canvas) {
    // The thumbnail may have different aspect ratio than the main image.
    // Force the main image proportions to avoid flicker.
    var time = Date.now();

    var mainImageLoadDelay = ImageView.ANIMATION_WAIT_INTERVAL;
    var mainImageSlide = slide;

    // Do not do slide-in animation when scrolling very fast.
    if (self.lastLoadTime_ &&
        (time - self.lastLoadTime_) < ImageView.FAST_SCROLL_INTERVAL) {
      mainImageSlide = 0;
    }
    self.lastLoadTime_ = time;

    if (canvas.width) {
      if (metadata.remote) {
        // We do not know the main image size, but chances are that it is large
        // enough. Show the thumbnail at the maximum possible scale.
        var bounds = self.viewport_.getScreenBounds();
        var scale = Math.min (bounds.width / canvas.width,
                              bounds.height / canvas.height);
        self.replace(canvas, slide,
            canvas.width * scale, canvas.height * scale, true /* preview */);
      } else {
        self.replace(canvas, slide,
            metadata.width, metadata.height, true /* preview */);
      }
      if (!mainImageSlide) mainImageLoadDelay = 0;
      mainImageSlide = 0;
    } else {
      // Thumbnail image load failed, loading the main image immediately.
      mainImageLoadDelay = 0;
    }
    loadMainImage(loadType, mainImageSlide, source,
        canvas.width != 0, mainImageLoadDelay);
  }

  function loadMainImage(loadType, slide, contentURL, previewShown, delay) {
    if (self.prefetchLoader_.isLoading(contentURL)) {
      // The image we need is already being prefetched. Initiating another load
      // would be a waste. Hijack the load instead by overriding the callback.
      self.prefetchLoader_.setCallback(
          displayMainImage.bind(null, loadType, slide, previewShown));

      // Swap the loaders so that the self.isLoading works correctly.
      var temp = self.prefetchLoader_;
      self.prefetchLoader_ = self.imageLoader_;
      self.imageLoader_ = temp;
      return;
    }
    self.prefetchLoader_.cancel();  // The prefetch was doing something useless.

    self.imageLoader_.load(
        contentURL,
        self.localImageTransformFetcher_,
        displayMainImage.bind(null, loadType, slide, previewShown),
        delay);
  }

  function displayMainImage(loadType, slide, previewShown, content) {
    if ((!loadingVideo && !content.width) ||
        (loadingVideo && !content.duration)) {
      loadType = ImageView.LOAD_TYPE_ERROR;
    }

    // If we already displayed the preview we should not replace the content if:
    //   1. The full content failed to load.
    //     or
    //   2. We are loading a video (because the full video is displayed in the
    //      same HTML element as the preview).
    if (!(previewShown &&
        (loadType == ImageView.LOAD_TYPE_ERROR ||
         loadType == ImageView.LOAD_TYPE_VIDEO_FILE))) {
      self.replace(content, slide);
    }

    if (loadType != ImageView.LOAD_TYPE_ERROR &&
        loadType != ImageView.LOAD_TYPE_CACHED_SCREEN) {
      ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('DisplayTime'));
    }
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('LoadMode'),
        loadType, ImageView.LOAD_TYPE_TOTAL);
    if (opt_callback) opt_callback(loadType);
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
 */
ImageView.prototype.prefetch = function(id, source) {
  var self = this;
  function prefetchDone(canvas) {
    if (canvas.width)
      self.contentCache_.putItem(id, canvas);
  }

  var cached = this.getReadyContent(id, source);
  if (cached) {
    prefetchDone(cached);
  } else if (FileType.getMediaType(source) == 'image') {
    // Evict the LRU item before we allocate the new canvas to avoid unneeded
    // strain on memory.
    this.contentCache_.evictLRU();

    this.prefetchLoader_.load(
        source,
        this.localImageTransformFetcher_,
        prefetchDone,
        ImageView.ANIMATION_WAIT_INTERVAL);
  }
};

ImageView.prototype.replaceContent_ = function(
    content, opt_reuseScreenCanvas, opt_width, opt_height, opt_preview) {

  if (content.constructor.name == 'HTMLVideoElement') {
    this.contentCanvas_ = null;
    this.videoElement_ = content;
    this.screenImage_ = content;
    this.screenImage_.className = 'image';
    return;
  }

  if (!opt_reuseScreenCanvas || !this.screenImage_ ||
      this.screenImage_.constructor.name == 'HTMLVideoElement') {
    this.screenImage_ = this.document_.createElement('canvas');
    this.screenImage_.className = 'image';
  }

  this.videoElement_ = null;
  this.contentCanvas_ = content;
  this.invalidateCaches();
  this.viewport_.setImageSize(
      opt_width || this.contentCanvas_.width,
      opt_height || this.contentCanvas_.height);
  this.viewport_.fitImage();
  this.viewport_.update();
  this.draw();

  if (opt_reuseScreenCanvas && !this.screenImage_.parentNode) {
    this.container_.appendChild(this.screenImage_);
  }

  this.preview_ = opt_preview;
  // If this is not a thumbnail, cache the content and the screen-scale image.
  if (this.hasValidImage()) {
    this.contentCache_.putItem(this.contentID_, this.contentCanvas_, true);
    this.screenCache_.putItem(this.contentID_, this.screenImage_);

    // TODO(kaznacheev): It is better to pass screenImage_ as it is usually
    // much smaller than contentCanvas_ and still contains the entire image.
    // Once we implement zoom/pan we should pass contentCanvas_ instead.
    this.updateThumbnail_(this.screenImage_);

    this.contentRevision_++;
    for (var i = 0; i != this.contentCallbacks_.length; i++) {
      try {
        this.contentCallbacks_[i]();
      } catch(e) {
        console.error(e);
      }
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
 * @param {HTMLCanvasElement|HTMLVideoElement} content
 * @param {number} opt_slide Slide-in animation direction.
 *           <0 for right-to-left, > 0 for left-to-right, 0 for no animation.
 */
ImageView.prototype.replace = function(
    content, opt_slide, opt_width, opt_height, opt_preview) {
  var oldScreenImage = this.screenImage_;

  this.replaceContent_(content, !opt_slide, opt_width, opt_height, opt_preview);

  opt_slide = opt_slide || 0;
  // TODO(kaznacheev): The line below is too obscure.
  // Refactor the whole 'slide' thing for clarity.
  if (!opt_slide && !this.getVideo()) return;

  var newScreenImage = this.screenImage_;

  function numToSlideAttr(num) {
    return num < 0 ? 'left' : num > 0 ? 'right' : 'center';
  }

  newScreenImage.setAttribute('fade', 'true');
  this.setTransform(newScreenImage, opt_slide);
  this.container_.appendChild(newScreenImage);

  setTimeout(function() {
    newScreenImage.removeAttribute('fade');
    this.setTransform(newScreenImage);
    if (oldScreenImage) {
      oldScreenImage.setAttribute('fade', 'true');
      this.setTransform(oldScreenImage, -opt_slide);
      setTimeout(function() {
        oldScreenImage.parentNode.removeChild(oldScreenImage);
      }, ImageView.ANIMATION_WAIT_INTERVAL);
    }
  }.bind(this), 0);
};

/**
 * @param {HTMLCanvasElement|HTMLVideoElement} element The element to transform.
 * @param {number} opt_slide The slide direction, positive for right.
 */
ImageView.prototype.setTransform = function(element, opt_slide) {
  var transform = '';
  if (element.constructor.name == 'HTMLCanvasElement' &&
      this.viewport_.getDevicePixelRatio() != 1) {
    var scale = 1 / this.viewport_.getDevicePixelRatio();
    transform += 'scale(' + scale + ') ';
  }
  if (opt_slide) {
    var shift = (opt_slide > 0) ? 40 : -40;
    transform += 'translate(' + shift + 'px, 0px)';
  }
  element.style.webkitTransform = transform;
};

/**
 * Transform the canvas to visualize an editing operation.
 *
 * @param {HTMLCanvasElement} canvas The canvas to transform.
 * @param {Rect} rect1 Source rectangle. If null no translation is performed.
 * @param {Rect} rect2 Target rectangle. If null no translation is performed.
 * @param {number} scale Transform scale.
 * @param {number} rotate90 Rotation in 90 degree increments.
 */
ImageView.prototype.setTransformWithEffect = function(
    canvas, rect1, rect2, scale, rotate90) {
  var ratio = this.viewport_.getDevicePixelRatio();
  var transform = '';
  if (rotate90) {
    transform += 'rotate(' + rotate90 * 90 + 'deg) ';
  }
  if (rect1 && rect2) {
    var dx = (rect1.left + rect1.width / 2) - (rect2.left + rect2.width / 2);
    var dy = (rect1.top + rect1.height / 2) - (rect2.top + rect2.height / 2);
    transform += 'translate(' + (dx / ratio) + 'px,' + (dy / ratio) + 'px) ';
  }
  transform += 'scale(' + (scale / ratio) + ')';

  canvas.style.webkitTransform = transform;
};

/**
 * Visualize crop or rotate operation. Hide the old image instantly, animate
 * the new image to visualize the operation.
 *
 * @param {HTMLCanvasElement} canvas New content canvas.
 * @param {Rect} imageCropRect The crop rectangle in image coordinates.
 *                             Null for rotation operations.
 * @param {number} rotate90 Rotation angle in 90 degree increments.
 */
ImageView.prototype.replaceAndAnimate = function(
    canvas, imageCropRect, rotate90) {
  var oldScale = this.viewport_.getScale();
  var deviceCropRect = imageCropRect && this.viewport_.screenToDeviceRect(
        this.viewport_.imageToScreenRect(imageCropRect));

  var oldScreenImage = this.screenImage_;
  this.replaceContent_(canvas);
  var newScreenImage = this.screenImage_;

  // Display the new canvas, initially transformed.
  var deviceFullRect = this.viewport_.getDeviceClipped();

  //Transform instantly.
  newScreenImage.style.webkitTransitionDuration = '0ms';
  this.setTransformWithEffect(
      newScreenImage,
      deviceCropRect,
      deviceFullRect,
      oldScale / this.viewport_.getScale(),
      -rotate90);

  oldScreenImage.parentNode.appendChild(newScreenImage);
  oldScreenImage.parentNode.removeChild(oldScreenImage);

  // Let the layout fire.
  setTimeout(function() {
    // Animated back to non-transformed state.
    newScreenImage.style.webkitTransitionDuration = '';
    this.setTransform(newScreenImage);
  }.bind(this), 0);
};

/**
 * Visualize "undo crop". Shrink the current image to the given crop rectangle
 * while fading in the new image.
 *
 * @param {HTMLCanvasElement} canvas New content canvas.
 * @param {Rect} imageCropRect The crop rectangle in image coordinates.
 */
ImageView.prototype.animateAndReplace = function(canvas, imageCropRect) {
  var deviceFullRect = this.viewport_.getDeviceClipped();
  var oldScale = this.viewport_.getScale();

  var oldScreenImage = this.screenImage_;
  this.replaceContent_(canvas);
  var newScreenImage = this.screenImage_;

  var deviceCropRect = this.viewport_.screenToDeviceRect(
        this.viewport_.imageToScreenRect(imageCropRect));

  newScreenImage.setAttribute('fade', 'true');
  oldScreenImage.parentNode.insertBefore(newScreenImage, oldScreenImage);

  // Animate to the transformed state.
  this.setTransformWithEffect(
      oldScreenImage,
      deviceCropRect,
      deviceFullRect,
      this.viewport_.getScale() / oldScale,
      0);

  setTimeout(function() { newScreenImage.removeAttribute('fade') }, 0);

  setTimeout(function() {
    oldScreenImage.parentNode.removeChild(oldScreenImage);
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
