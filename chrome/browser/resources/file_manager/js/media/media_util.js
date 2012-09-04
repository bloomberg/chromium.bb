// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} url File URL.
 * @param {Object} opt_metadata Metadata object.
 * @param {string} opt_mediaType Media type.
 * @constructor
 */
function ThumbnailLoader(url, opt_metadata, opt_mediaType) {
  this.mediaType_ = opt_mediaType || FileType.getMediaType(url);

  if (!opt_metadata) {
    this.thumbnailUrl_ = url;  // Use the URL directly.
    return;
  }

  if (opt_metadata.gdata) {
    var apps = opt_metadata.gdata.driveApps;
    if (apps.length > 0 && apps[0].docIcon) {
      this.fallbackUrl_ = apps[0].docIcon;
    }
  }

  if (opt_metadata.thumbnail && opt_metadata.thumbnail.url) {
    this.thumbnailUrl_ = opt_metadata.thumbnail.url;
    this.transform_ = opt_metadata.thumbnail.transform;
  } else if (FileType.isImage(url) &&
      ThumbnailLoader.canUseImageUrl_(opt_metadata)) {
    this.thumbnailUrl_ = url;
    this.transform_ = opt_metadata.media && opt_metadata.media.imageTransform;
  } else if (this.fallbackUrl_) {
    // Use fallback as the primary thumbnail.
    this.thumbnailUrl_ = this.fallbackUrl_;
    this.fallbackUrl_ = null;
  } // else the generic thumbnail based on the media type will be used.
}

/**
 * Files with more pixels won't have thumbnails.
 */
ThumbnailLoader.MAX_PIXEL_COUNT = 1 << 21; // 2 MPix

/**
 * Files of bigger size won't have thumbnails.
 */
ThumbnailLoader.MAX_FILE_SIZE = 1 << 20; // 1 Mb

/**
 * If an image file does not have an embedded thumbnail we might want to use
 * the image itself as a thumbnail. If the image is too large it hurts
 * the performance very much so we allow it only for moderately sized files.
 *
 * @param {Object} metadata Metadata object
 * @return {boolean} Whether it is OK to use the image url for a preview.
 * @private
 */
ThumbnailLoader.canUseImageUrl_ = function(metadata) {
  return (metadata.filesystem && metadata.filesystem.size &&
      metadata.filesystem.size <= ThumbnailLoader.MAX_FILE_SIZE) ||
     (metadata.media && metadata.media.width && metadata.media.height &&
      metadata.media.width * metadata.media.height <=
          ThumbnailLoader.MAX_PIXEL_COUNT);
};

/**
 *
 * @param {HTMLElement} box Container element.
 * @param {boolean} fill True if fill, false if fit.
 * @param {function(HTMLImageElement, object} opt_onSuccess Success callback,
 *   accepts the image and the transform.
 * @param {function} opt_onError Error callback.
 */
ThumbnailLoader.prototype.load = function(
    box, fill, opt_onSuccess, opt_onError) {
  if (!this.thumbnailUrl_) {
    // Relevant CSS rules are in file_types.css.
    box.setAttribute('generic-thumbnail', this.mediaType_);
    return;
  }

  this.image_ = box.ownerDocument.createElement('img');
  this.image_.onload = function() {
    this.attachImage(box, fill);
    if (opt_onSuccess)
      opt_onSuccess(this.image_, this.transform_);
  }.bind(this);
  this.image_.onerror = function() {
    if (opt_onError)
      opt_onError();
    if (this.fallbackUrl_)
      new ThumbnailLoader(this.fallbackUrl_, null, this.mediaType_).
          load(box, fill, opt_onSuccess);
  }.bind(this);

  if (this.image_.src == this.thumbnailUrl_) {
    console.warn('Thumnbnail already loaded: ' + this.thumbnailUrl_);
    return;
  }

  this.image_.src = this.thumbnailUrl_;
};

/**
 * @return {boolean} True if a valid image is loaded.
 */
ThumbnailLoader.prototype.hasValidImage = function() {
  return !!(this.image_ && this.image_.width && this.image_.height);
};

/**
 * @return {boolean} True if the image is rotated 90 degrees left or right.
 * @private
 */
ThumbnailLoader.prototype.isRotated_ = function() {
  return this.transform_ && (this.transform_.rotate90 % 2 == 1);
};

/**
 * @return {number} Image width (corrected for rotation).
 */
ThumbnailLoader.prototype.getWidth = function() {
  return this.isRotated_() ? this.image_.height : this.image_.width;
};

/**
 * @return {number} Image height (corrected for rotation).
 */
ThumbnailLoader.prototype.getHeight = function() {
  return this.isRotated_() ? this.image_.width : this.image_.height;
};

/**
 * Load an image but do not attach it.
 *
 * @param {function} callback Callback.
 */
ThumbnailLoader.prototype.loadDetachedImage = function(callback) {
  if (!this.thumbnailUrl_) {
    callback();
    return;
  }

  this.image_ = new Image();
  this.image_.onload = this.image_.onerror = callback;
  this.image_.src = this.thumbnailUrl_;
};

/**
 * Attach the image to a given element.
 * @param {Element} container Parent element.
 * @param {boolean} fill True for fill, false for fit.
 */
ThumbnailLoader.prototype.attachImage = function(container, fill) {
  if (!this.image_) {
    container.setAttribute('generic-thumbnail', this.mediaType_);
    return;
  }

  util.applyTransform(container, this.transform_);
  ThumbnailLoader.centerImage_(container, this.image_, fill, this.isRotated_());
  if (this.image_.parentNode != container) {
    container.textContent = '';
    container.appendChild(this.image_);
  }
};

/**
 * Update the image style to fit/fill the container.
 *
 * Using webkit center packing does not align the image properly, so we need
 * to wait until the image loads and its dimensions are known, then manually
 * position it at the center.
 *
 * @param {HTMLElement} box Containing element.
 * @param {HTMLImageElement} img Image element.
 * @param {boolean} fill True: the image should fill the entire container,
 *                       false: the image should fully fit into the container.
 * @param {boolean} rotate True if the image should be rotated 90 degrees.
 * @private
 */
ThumbnailLoader.centerImage_ = function(box, img, fill, rotate) {
  var imageWidth = img.width;
  var imageHeight = img.height;

  var fractionX;
  var fractionY;

  var boxWidth = box.clientWidth;
  var boxHeight = box.clientHeight;

  if (boxWidth && boxHeight) {
    // When we know the box size we can position the image correctly even
    // in a non-square box.
    var fitScaleX = (rotate ? boxHeight : boxWidth) / imageWidth;
    var fitScaleY = (rotate ? boxWidth : boxHeight) / imageHeight;

    var scale = fill ?
        Math.max(fitScaleX, fitScaleY) :
        Math.min(fitScaleX, fitScaleY);

    scale = Math.min(scale, 1);  // Never overscale.

    fractionX = imageWidth * scale / boxWidth;
    fractionY = imageHeight * scale / boxHeight;
  } else {
    // We do not know the box size so we assume it is square.
    // Compute the image position based only on the image dimensions.
    // First try vertical fit or horizontal fill.
    fractionX = imageWidth / imageHeight;
    fractionY = 1;
    if ((fractionX < 1) == !!fill) {  // Vertical fill or horizontal fit.
      fractionY = 1 / fractionX;
      fractionX = 1;
    }
  }

  function percent(fraction) {
    return (fraction * 100).toFixed(2) + '%';
  }

  img.style.width = percent(fractionX);
  img.style.height = percent(fractionY);
  img.style.left = percent((1 - fractionX) / 2);
  img.style.top = percent((1 - fractionY) / 2);
};
