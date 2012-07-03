// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} imageUrl Image URL
 * @param {Object} opt_metadata Metadata object
 * @constructor
 */
function ThumbnailLoader(imageUrl, opt_metadata) {
  var genericIconUrl = FileType.getPreviewArt(FileType.getMediaType(imageUrl));
  if (opt_metadata && opt_metadata.gdata) {
    var apps = opt_metadata.gdata.driveApps;
    if (apps.length > 0 && apps[0].docIcon) {
      genericIconUrl = apps[0].docIcon;
    }
  }

  if (!opt_metadata) {
    this.thumbnailUrl_ = imageUrl;
  } else if (opt_metadata.thumbnail && opt_metadata.thumbnail.url) {
    this.thumbnailUrl_ = opt_metadata.thumbnail.url;
    this.fallbackUrl_ = genericIconUrl;
    this.transform_ = opt_metadata.thumbnail.transform;
  } else if (FileType.isImage(imageUrl) &&
      ThumbnailLoader.canUseImageUrl_(opt_metadata)) {
    this.thumbnailUrl_ = imageUrl;
    this.fallbackUrl_ = genericIconUrl;
    this.transform_ = opt_metadata.media && opt_metadata.media.imageTransform;
  } else {
    this.thumbnailUrl_ = genericIconUrl;
  }
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
  var img = box.ownerDocument.createElement('img');
  img.onload = function() {
    util.applyTransform(box, this.transform_);
    ThumbnailLoader.centerImage_(box, img, fill,
        this.transform_ && (this.transform_.rotate90 % 2 == 1));
    if (opt_onSuccess)
      opt_onSuccess(img, this.transform_);
    box.textContent = '';
    box.appendChild(img);
  }.bind(this);
  img.onerror = function() {
    if (opt_onError)
      opt_onError();
    if (this.fallbackUrl_)
      new ThumbnailLoader(this.fallbackUrl_).load(box, fill, opt_onSuccess);
  }.bind(this);

  if (img.src == this.thumbnailUrl_) {
    console.warn('Thumnbnail already loaded: ' + this.thumbnailUrl_);
    return;
  }

  img.src = this.thumbnailUrl_;
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
