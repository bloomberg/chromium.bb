// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  var constructor = ImageEncoder.metadataEncoders[metadata.mimeType] ||
      ImageEncoder.MetadataEncoder;
  return new constructor(metadata);
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
  encoder.setImageData(canvas);
  encoder.setThumbnailData(ImageEncoder.createThumbnail(canvas), quality || 1);
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
  ImageEncoder.buildBlob(blobBuilder, canvas, metadataEncoder, quality);
  return blobBuilder.getBlob(metadataEncoder.getMetadata().mimeType);
};

/**
 * Build a blob containing the encoded image with metadata inserted.
 * @param {BlobBuilder} blobBuilder
 * @param {HTMLCanvasElement} canvas The canvas with the image to be encoded.
 * @param {ImageEncoder.MetadataEncoder} metadataEncoder
 * @param {Number} quality (0..1], Encoding quality, defaults to 0.9.
 */
ImageEncoder.buildBlob = function(
    blobBuilder, canvas, metadataEncoder, quality) {

  // Contrary to what one might think 1.0 is not a good default. Opening and
  // saving an typical photo taken with consumer camera increases its file size
  // by 50-100%.
  // Experiments show that 0.9 is much better. It shrinks some photos a bit,
  // keeps others about the same size, but does not visibly lower the quality.
  quality = quality || 0.9;

  ImageUtil.trace.resetTimer('dataurl');
  // WebKit does not support canvas.toBlob yet so canvas.toDataURL is
  // the only way to use the Chrome built-in image encoder.
  var dataURL =
      canvas.toDataURL(metadataEncoder.getMetadata().mimeType, quality);
  ImageUtil.trace.reportTimer('dataurl');

  var encodedImage = ImageEncoder.decodeDataURL(dataURL);

  var encodedMetadata = metadataEncoder.encode();

  ImageUtil.trace.resetTimer('blob');
  if (encodedMetadata.byteLength != 0) {
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
 * Decode a dataURL into a binary string containing the encoded image.
 *
 * Why return a string? Calling atob and having the rest of the code deal
 * with a string is several times faster than decoding base64 in Javascript.
 *
 * @param {String} dataURL
 * @return {String} A binary string (char codes are the actual byte values).
 */
ImageEncoder.decodeDataURL = function(dataURL) {
  // Skip the prefix ('data:image/<type>;base64,')
  var base64string = dataURL.substring(dataURL.indexOf(',') + 1);
  return atob(base64string);
};

/**
 * Return a thumbnail for an image.
 * @param {HTMLCanvasElement} canvas Original image.
 * @param {Number} opt_shrinkage Thumbnail should be at least this much smaller
 *                               than the original image (in each dimension).
 * @return {HTMLCanvasElement} Thumbnail canvas
 */
ImageEncoder.createThumbnail = function(canvas, opt_shrinkage) {
  const MAX_THUMBNAIL_DIMENSION = 320;

  opt_shrinkage = Math.max(opt_shrinkage || 4,
                       canvas.width / MAX_THUMBNAIL_DIMENSION,
                       canvas.height / MAX_THUMBNAIL_DIMENSION);

  var thumbnailCanvas = canvas.ownerDocument.createElement('canvas');
  thumbnailCanvas.width = Math.round(canvas.width / opt_shrinkage);
  thumbnailCanvas.height = Math.round(canvas.height / opt_shrinkage);

  var context = thumbnailCanvas.getContext('2d');
  context.drawImage(canvas,
      0, 0, canvas.width, canvas.height,
      0, 0, thumbnailCanvas.width, thumbnailCanvas.height);

  return thumbnailCanvas;
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
 * Serves as a default metadata encoder for images that none of the metadata
 * parsers recognized.
 *
 * @param {Object} original_metadata
 */
ImageEncoder.MetadataEncoder = function(original_metadata) {
  this.metadata_ = ImageUtil.deepCopy(original_metadata) || {};
  if (this.metadata_.mimeType != 'image/jpeg') {
    // Chrome can only encode JPEG and PNG. Force PNG mime type so that we
    // can save to file and generate a thumbnail.
    this.metadata_.mimeType = 'image/png';
  }
};

ImageEncoder.MetadataEncoder.prototype.getMetadata = function() {
  return this.metadata_;
};

/**
 * @param {HTMLCanvasElement|Object} canvas Canvas or or anything with
 *                                          width and height properties.
 */
ImageEncoder.MetadataEncoder.prototype.setImageData = function(canvas) {
  this.metadata_.width = canvas.width;
  this.metadata_.height = canvas.height;
};

/**
 * @param {HTMLCanvasElement} canvas
 * @param {number} quality
 */
ImageEncoder.MetadataEncoder.prototype.setThumbnailData =
    function(canvas, quality) {
  this.metadata_.thumbnailURL =
      canvas.toDataURL(this.metadata_.mimeType, quality);
};

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
ImageEncoder.MetadataEncoder.prototype.encode = function() {
  return new Uint8Array(0).buffer;
};
