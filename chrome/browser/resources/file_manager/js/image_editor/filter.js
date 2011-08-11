// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A namespace for image filter utilities.
 */
var filter = {};

/**
 * Create a filter from name and options.
 *
 * @param {string} name Maps to a filter method name.
 * @param {Object} options A map of filter-specific options.
 * @return {function(ImageData,ImageData,number,number)}
 */
filter.create = function(name, options) {
  var filterFunc = filter[name](options);
  return function() {
    var time = Date.now();
    filterFunc.apply(null, arguments);
    var dst = arguments[0];
    var mPixPerSec = dst.width * dst.height / 1000 / (Date.now() - time);
    ImageUtil.trace.report(name, Math.round(mPixPerSec * 10) / 10 + 'Mps');
  }
};

/**
 * Apply a filter to a image by splitting it into strips.
 *
 * To be used with large images to avoid freezing up the UI.
 *
 * @param {CanvasRenderingContext2D} context
 * @param {function(ImageData,ImageData,number,number)} filterFunc
 * @param {function(number,number} progressCallback
 * @param {number} maxPixelsPerStrip
 */
filter.applyByStrips = function(
    context, filterFunc, progressCallback, maxPixelsPerStrip) {
  var source = context.getImageData(
      0, 0, context.canvas.width, context.canvas.height);

  var stripCount = Math.ceil (source.width * source.height /
      (maxPixelsPerStrip || 1000000));  // 1 Mpix is a reasonable default.

  var strip = context.getImageData(0, 0,
      source.width, Math.ceil (source.height / stripCount));

  var offset = 0;

  function filterStrip() {
    // If the strip overlaps the bottom of the source image we cannot shrink it
    // and we cannot fill it partially (since canvas.putImageData always draws
    // the entire buffer).
    // Instead we move the strip up several lines (converting those lines
    // twice is a small price to pay).
    if (offset > source.height - strip.height) {
      offset = source.height - strip.height;
    }

    filterFunc(strip, source, 0, offset);
    context.putImageData(strip, 0, offset);

    offset += strip.height;
    progressCallback(offset, source.height);

    if (offset < source.height) {
      setTimeout(filterStrip, 0);
    }
  }

  filterStrip();
};

/**
 * Return a color histogram for an image.
 *
 * @param {ImageData} imageData
 * @return {{r: Array.<number>, g: Array.<number>, b: Array.<number>}}
 */
filter.getHistogram = function(imageData) {
  var r = [];
  var g = [];
  var b = [];

  for (var i = 0; i != 256; i++) {
    r.push(0);
    g.push(0);
    b.push(0);
  }

  var data = imageData.data;
  var maxIndex = 4 * imageData.width * imageData.height;
  for (var index = 0; index != maxIndex;) {
    r[data[index++]]++;
    g[data[index++]]++;
    b[data[index++]]++;
    index++;
  }

  return { r: r, g: g, b: b };
};

/**
 * Compute the function for every integer value from 0 up to maxArg.
 *
 * Rounds and clips the results to fit the [0..255] range.
 * Useful to speed up pixel manipulations.
 *
 * @param {number} maxArg Maximum argument value (inclusive).
 * @param {function(number): number} func
 * @return {Array.<number>} Computed results
 */
filter.precompute = function(maxArg, func) {
  var results = [];
  for (var arg = 0; arg <= maxArg; arg ++) {
    results.push(Math.max(0, Math.min(0xFF, Math.round(func(arg)))));
  }
  return results;
};

/**
 * Convert pixels by applying conversion tables to each channel individually.
 *
 * @param {Array.<number>} rMap Red channel conversion table.
 * @param {Array.<number>} gMap Green channel conversion table.
 * @param {Array.<number>} bMap Blue channel conversion table.
 * @param {ImageData} dst Destination image data. Can be smaller than the
 *                        source, must completely fit inside the source.
 * @param {ImageData} src Source image data.
 * @param {number} offsetX Horizontal offset of dst relative to src.
 * @param {number} offsetY Vertical offset of dst relative to src.
 */
filter.mapPixels = function(rMap, gMap, bMap, dst, src, offsetX, offsetY) {
  var dstData = dst.data;
  var dstWidth = dst.width;
  var dstHeight = dst.height;

  var srcData = src.data;
  var srcWidth = src.width;
  var srcHeight = src.height;

  if (offsetX < 0 || offsetX + dstWidth > srcWidth ||
      offsetY < 0 || offsetY + dstHeight > srcHeight)
      throw new Error('Invalid offset');

  var dstIndex = 0;
  for (var y = 0; y != dstHeight; y++) {
    var srcIndex = (offsetX + (offsetY + y)* srcWidth)* 4;
    for (var x = 0; x != dstWidth; x++ ) {
      dstData[dstIndex++] = rMap[srcData[srcIndex++]];
      dstData[dstIndex++] = gMap[srcData[srcIndex++]];
      dstData[dstIndex++] = bMap[srcData[srcIndex++]];
      dstIndex++;
      srcIndex++;
    }
  }
};

filter.FIXED_POINT_SHIFT = 16;
filter.MAX_SHIFTED_VALUE = 0xFF << filter.FIXED_POINT_SHIFT;

filter.floatToFixedPoint = function(x) {
  return Math.round((x || 0) * (1 << filter.FIXED_POINT_SHIFT));
};

/**
 * Perform an image convolution with a symmetrical 5x5 matrix:
 *
 *  0  0 w3  0  0
 *  0 w2 w1 w2  0
 * w3 w1 w0 w1 w3
 *  0 w2 w1 w2  0
 *  0  0 w3  0  0
 *
 * For performance reasons the weights are in fixed point format
 * (left shifted by filter.FIXED_POINT_SHIFT).
 *
 * @param {number} w0 See the picture above
 * @param {number} w1 See the picture above
 * @param {number} w2 See the picture above
 * @param {number} w3 See the picture above
 * @param {ImageData} dst Destination image data. Can be smaller than the
 *                        source, must completely fit inside the source.
 * @param {ImageData} src Source image data.
 * @param {number} offsetX Horizontal offset of dst relative to src.
 * @param {number} offsetY Vertical offset of dst relative to src.
 */
filter.convolve5x5 = function(w0, w1, w2, w3, dst, src, offsetX, offsetY) {
  var dstData = dst.data;
  var dstWidth = dst.width;
  var dstHeight = dst.height;
  var dstStride = dstWidth * 4;

  var srcData = src.data;
  var srcWidth = src.width;
  var srcHeight = src.height;
  var srcStride = srcWidth * 4;
  var srcStride2 = srcStride * 2;

  if (offsetX < 0 || offsetX + dstWidth > srcWidth ||
      offsetY < 0 || offsetY + dstHeight > srcHeight)
      throw new Error('Invalid offset');

  w0 = filter.floatToFixedPoint(w0);
  w1 = filter.floatToFixedPoint(w1);
  w2 = filter.floatToFixedPoint(w2);
  w3 = filter.floatToFixedPoint(w3);

  var margin = 2;

  var startX = Math.max(0, margin - offsetX);
  var endX = Math.min(dstWidth, srcWidth - margin - offsetX);

  var startY = Math.max(0, margin - offsetY);
  var endY = Math.min(dstHeight, srcHeight - margin - offsetY);

  for (var y = startY; y != endY; y++) {
    var dstIndex = y * dstStride + startX * 4;
    var srcIndex = (y + offsetY) * srcStride + (startX + offsetX) * 4;

    for (var x = startX; x != endX; x++ ) {
      for (var c = 0; c != 3; c++) {
        var sum = w0 * srcData[srcIndex] +
                  w1 * (srcData[srcIndex - 4] +
                        srcData[srcIndex + 4] +
                        srcData[srcIndex - srcStride] +
                        srcData[srcIndex + srcStride]) +
                  w2 * (srcData[srcIndex - srcStride - 4] +
                        srcData[srcIndex + srcStride - 4] +
                        srcData[srcIndex - srcStride + 4] +
                        srcData[srcIndex + srcStride + 4]) +
                  w3 * (srcData[srcIndex - 8] +
                        srcData[srcIndex + 8] +
                        srcData[srcIndex - srcStride2] +
                        srcData[srcIndex + srcStride2]);
        if (sum < 0)
          dstData[dstIndex++] = 0;
        else if (sum > Filter.MAX_SHIFTED_VALUE)
          dstData[dstIndex++] = 0xFF;
        else
          dstData[dstIndex++] = sum >> Filter.FIXED_POINT_SHIFT;
        srcIndex++;
      }
      srcIndex++;
      dstIndex++;
    }
  }
};

/**
 * Return a convolution filter function bound to specific weights.
 *
 * @param {Array.<number>} weights Weights for the convolution matrix.
 * @return {function(ImageData,ImageData,number,number)}
 */
filter.convolution5x5 = function(weights) {
  var total = 0;
  for (var i = 0; i != weights.length; i++) {
    total += weights[i] * (i ? 4 : 1);
  }

  for (i = 0; i != weights.length; i++) {
    weights[i] /= total;
  }

  return filter.convolve5x5.bind(
      null, weights[0], weights[1], weights[2], weights[3]);
};

/**
 * Return a blur filter.
 * @param {Object} options
 * @return {function(ImageData,ImageData,number,number)}
 */
filter.blur = function(options) {
  if (options.radius == 1)
    return filter.convolution5x5(
        [1, options.strength]);
  else if (options.radius == 2)
    return filter.convolution5x5(
        [1, options.strength, options.strength]);
  else
    return filter.convolution5x5(
        [1, options.strength, options.strength, options.strength]);
};

/**
 * Return a sharpen filter.
 * @param {Object} options
 * @return {function(ImageData,ImageData,number,number)}
 */
filter.sharpen = function(options) {
  if (options.radius == 1)
    return filter.convolution5x5(
        [5, -options.strength]);
  else if (options.radius == 2)
    return filter.convolution5x5(
        [10, -options.strength, -options.strength]);
  else
    return filter.convolution5x5(
        [15, -options.strength, -options.strength, -options.strength]);
};

/**
 * Return an exposure filter.
 * @param {Object} options
 * @return {function(ImageData,ImageData,number,number)}
 */
filter.exposure = function(options) {
  var pixelMap = filter.precompute(
    255,
    function(value) {
     if (options.brightness > 0) {
       value *= (1 + options.brightness);
     } else {
       value += (0xFF - value) * options.brightness;
     }
     return 0x80 +
         (value - 0x80) * Math.tan((options.contrast + 1) * Math.PI / 4);
    });

  return filter.mapPixels.bind(null, pixelMap, pixelMap, pixelMap);
};

/**
 * Return a color autofix filter.
 * @param {Object} options
 * @return {function(ImageData,ImageData,number,number)}
 */
filter.autofix = function(options) {
  return filter.mapPixels.bind(null,
      filter.autofix.stretchColors(options.histogram.r),
      filter.autofix.stretchColors(options.histogram.g),
      filter.autofix.stretchColors(options.histogram.b));
};

/**
 * Return a conversion table that stretches the range of colors used
 * in the image to 0..255.
 * @param {Array.<number>} channelHistogram
 * @return {Array.<number>}
 */
filter.autofix.stretchColors = function(channelHistogram) {
  var first = 0;
  while (first <= 255 && channelHistogram[first] == 0)
    first++;

  var last = 255;
  while (last > first && channelHistogram[last] == 0)
    last--;

  return filter.precompute(
      255,
      last == first ?
          function(x) { return x } :
          function(x) { return (x - first) / (last - first) * 255 }
  );
};
