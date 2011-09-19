// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A namespace class for image encoding functions. All methods are static.
 */
function ImageEncoder() {}

ImageEncoder.MAX_THUMBNAIL_DIMENSION = 320;

ImageEncoder.metadataEncoders = {};

ImageEncoder.registerMetadataEncoder = function(constructor, mimeType) {
  ImageEncoder.metadataEncoders[mimeType] = constructor;
};

/**
 * Create a metadata encoder.
 *
 * The encoder will own and modify a copy of the original metadata.
 *
 * @param {Object} metadata Original metadata
 * @return {ImageEncoder.MetadataEncoder}
 */
ImageEncoder.createMetadataEncoder = function(metadata) {
  var constructor = ImageEncoder.metadataEncoders[metadata.mimeType];
  return constructor ? new constructor(ImageUtil.deepCopy(metadata)) : null;
};


/**
 * Create a metadata encoder object holding a copy of metadata
 * modified according to the properties of the supplied image.
 *
 * @param {Object} metadata Original metadata
 * @param {HTMLCanvasElement} canvas
 * @param {number} quality Encoding quality (defaults to 1)
 */
ImageEncoder.encodeMetadata = function(metadata, canvas, quality) {
  var encoder = ImageEncoder.createMetadataEncoder(metadata);
  if (encoder) {
    encoder.setImageData(canvas);
    ImageEncoder.encodeThumbnail(canvas, encoder, quality);
  }
  return encoder;
};


/**
 * Return a blob with the encoded image with metadata inserted.
 * @param {HTMLCanvasElement} canvas The canvas with the image to be encoded.
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder
 * @param {number} quality
 * @return {Blob}
 */
ImageEncoder.getBlob = function(canvas, metadataEncoder, quality) {
  var blobBuilder = new WebKitBlobBuilder();
  ImageEncoder.buildBlob(blobBuilder, canvas,
      metadataEncoder, metadataEncoder.getMetadata().mimeType, quality);
  return blobBuilder.getBlob();
};

/**
 * Build a blob containing the encoded image with metadata inserted.
 * @param {BlobBuilder} blobBuilder
 * @param {HTMLCanvasElement} canvas The canvas with the image to be encoded.
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder
 * @param {String} mimeType
 * @param {Number} quality (0..1], Encoding quality, default is 0.5
 */
ImageEncoder.buildBlob = function(
    blobBuilder, canvas, metadataEncoder, mimeType, quality) {

  quality = quality || 0.5;

  var encodedImage = ImageEncoder.encodeImage(canvas, mimeType, quality);

  ImageUtil.trace.resetTimer('blob');
  if (metadataEncoder) {
    var metadataRange = metadataEncoder.findInsertionRange(encodedImage);

    blobBuilder.append(ImageEncoder.stringToArrayBuffer(
        encodedImage, 0, metadataRange.from));

    blobBuilder.append(metadataEncoder.encode());

    blobBuilder.append(ImageEncoder.stringToArrayBuffer(
        encodedImage, metadataRange.to, encodedImage.length));
  } else {
    blobBuilder.append(ImageEncoder.stringToArrayBuffer(
        encodedImage, 0, encodedImage.length));
  }
  ImageUtil.trace.reportTimer('blob');
};

/**
 * Return a string with encoded image.
 *
 * Why return a string? WebKits does not support canvas.toBlob yet so the only
 * way to use the Chrome built-in encoder is canvas.toDataURL which returns
 * base64-encoded string. Calling atob and having the rest of the code deal
 * with a string is several times faster than decoding base64 in Javascript.
 *
 * @param {HTMLCanvasElement} canvas
 * @param {String} mimeType
 * @param {Number} quality
 * @return {String}
 */
ImageEncoder.encodeImage = function(canvas, mimeType, quality) {
  var dataURL = canvas.toDataURL(mimeType, quality);
  var base64string = dataURL.substring(dataURL.indexOf(',') + 1);
  return atob(base64string);
};

/**
 * Create a thumbnail and pass it to the metadata encoder.
 * @param {HTMLCanvasElement} canvas
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder
 * @param {number} quality Image encoding quality
 */
ImageEncoder.encodeThumbnail = function(canvas, metadataEncoder, quality) {
  var thumbnailCanvas;
  var thumbnailURL;

  var pixelCount = canvas.width * canvas.height;

  // Is the image large enough to need a thumbnail?
  if (pixelCount > Math.pow(ImageEncoder.MAX_THUMBNAIL_DIMENSION, 2)) {
    thumbnailCanvas = ImageEncoder.createThumbnail(canvas, 4);
    // Encode the thumbnail with the quality a little lower than the original.
    // Empirical formula with reasonable behavior:
    // 10K for 1Mpix, 30K for 5Mpix, 50K for 9Mpix and up.
    var maxEncodedSize = 5000 * Math.min(10, 1 + pixelCount / 1000000);

    var mimeType = metadataEncoder.getMetadata().mimeType;

    thumbnailURL = ImageEncoder.getThumbnailURL(
        thumbnailCanvas, mimeType, quality * 0.9, maxEncodedSize);
  }

  metadataEncoder.setThumbnailData(thumbnailCanvas, thumbnailURL);
};

/**
 * Return a thumbnail for an image.
 * @param {HTMLCanvasElement} canvas Original image.
 * @param {Number} shrinkage Thumbnail should at least this much smaller than
 *                           the original image (in each dimension).
 * @return {HTMLCanvasElement} Thumbnail canvas
 */
ImageEncoder.createThumbnail = function(canvas, shrinkage) {
  const MAX_THUMBNAIL_DIMENSION = 320;

  shrinkage = Math.max(shrinkage,
                       canvas.width / MAX_THUMBNAIL_DIMENSION,
                       canvas.height / MAX_THUMBNAIL_DIMENSION);

  var thumbnailCanvas = canvas.ownerDocument.createElement('canvas');
  thumbnailCanvas.width = Math.round(canvas.width / shrinkage);
  thumbnailCanvas.height = Math.round(canvas.height / shrinkage);

  var context = thumbnailCanvas.getContext('2d');
  context.drawImage(canvas,
      0, 0, canvas.width, canvas.height,
      0, 0, thumbnailCanvas.width, thumbnailCanvas.height);

  return thumbnailCanvas;
};

/**
 * Return a data URL with the image encoded.
 * @param {HTMLCanvasElement} canvas.
 * @param {String} mimeType
 * @param {Number} maxQuality Maximum encoding quality (actual quality can be
 *                            lower to meet the size limit.
 * @param {Number} maxEncodedSize Maximum size of the binary encoded data.
 */
ImageEncoder.getThumbnailURL = function(
    canvas, mimeType, maxQuality, maxEncodedSize) {

  const DATA_URL_PREFIX = 'data:' + mimeType + ';base64,';
  const BASE64_BLOAT = 4 / 3;
  var maxDataURLLength =
      DATA_URL_PREFIX.length + Math.ceil(maxEncodedSize * BASE64_BLOAT);

  for (var quality = maxQuality; quality > 0.2; quality *= 0.8) {
    var dataURL = canvas.toDataURL(mimeType, quality);
    if (dataURL.length <= maxDataURLLength)
      return dataURL;
  }

  console.error('Unable to create thumbnail');
  return null;
};

ImageEncoder.stringToArrayBuffer = function(string, from, to) {
  var size = to - from;
  var array = new Uint8Array(size);
  for (var i = 0; i != size; i++) {
    array[i] = string.charCodeAt(from + i);
  }
  return array.buffer;
};

/**
 * A base class for a metadata encoder.
 *
 * @param {Object} original_metadata
 */
ImageEncoder.MetadataEncoder = function(original_metadata) {
  this.metadata_ = ImageUtil.deepCopy(original_metadata) || {};
};

ImageEncoder.MetadataEncoder.prototype.getMetadata = function() {
  return this.metadata_;
};

/**
 * @param {HTMLCanvasElement|Object} canvas Canvas or or anything with
 *                                          width and height properties.
 */
ImageEncoder.MetadataEncoder.prototype.setImageData = function(canvas) {};

/**
 * @param {HTMLCanvasElement|Object} canvas Canvas or or anything with
 *                                          width and height properties.
 * @param {String} dataUrl Data url containing the thumbnail.
 */
ImageEncoder.MetadataEncoder.prototype.
    setThumbnailData = function(canvas, dataUrl) {};

/**
 * Return a range where the metadata is (or should be) located.
 * @param {String} encodedImage
 * @return {Object} An object with from and to properties.
 */
ImageEncoder.MetadataEncoder.prototype.
    findInsertionRange = function(encodedImage) { return {from: 0, to: 0} };

/**
 * Return serialized metadata ready to write to an image file.
 * The return type is optimized for passing to Blob.append.
 * @return {ArrayBuffer}
 */
ImageEncoder.MetadataEncoder.prototype.encode = function() { return null };