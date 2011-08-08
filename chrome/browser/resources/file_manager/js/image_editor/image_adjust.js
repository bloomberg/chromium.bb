// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The base class for simple filters that only modify the image content
 * but do not modify the image dimensions.
 */
ImageEditor.Mode.Adjust = function(displayName, filterFunc) {
  ImageEditor.Mode.call(this, displayName);
  this.filterFunc_ = filterFunc;
}

ImageEditor.Mode.Adjust.prototype = {__proto__: ImageEditor.Mode.prototype};

ImageEditor.Mode.Adjust.prototype.rollback = function() {
  if (!this.backup_) return; // Did not do anything yet.
  this.getContent().drawImageData(this.backup_, 0, 0);
  this.backup_ = null;
  this.repaint();
};

ImageEditor.Mode.Adjust.prototype.update = function(options) {
  if (!this.backup_) {
    this.backup_ = this.getContent().copyImageData();
    this.scratch_ = this.getContent().copyImageData();
  }

  ImageUtil.trace.resetTimer('filter');
  this.filterFunc_(this.scratch_, this.backup_, options);
  ImageUtil.trace.reportTimer('filter');
  this.getContent().drawImageData(this.scratch_, 0, 0);
  this.repaint();
};

/**
 * A simple filter that multiplies every component of a pixel by some number.
 */
ImageEditor.Mode.Brightness = function() {
  ImageEditor.Mode.Adjust.call(
      this, 'Brightness', ImageEditor.Mode.Brightness.filter);
}

ImageEditor.Mode.Brightness.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

ImageEditor.Mode.register(ImageEditor.Mode.Brightness);

ImageEditor.Mode.Brightness.UI_RANGE = 100;

ImageEditor.Mode.Brightness.prototype.createTools = function(toolbar) {
  toolbar.addRange(
      'brightness',
      -ImageEditor.Mode.Brightness.UI_RANGE,
      0,
      ImageEditor.Mode.Brightness.UI_RANGE);
};

ImageEditor.Mode.Brightness.filter = function(dst, src, options) {
  // Translate from -100..100 range to 1/5..5
  var factor =
      Math.pow(5, options.brightness / ImageEditor.Mode.Brightness.UI_RANGE);

  var dstData = dst.data;
  var srcData = src.data;
  var width = src.width;
  var height = src.height;

  function scale(value) {
    return value * factor;
  }

  var values = ImageUtil.precomputeByteFunction(scale, 255);

  var index = 0;
  for (var y = 0; y != height; y++) {
    for (var x = 0; x != width; x++ ) {
      dstData[index] = values[srcData[index]]; index++;
      dstData[index] = values[srcData[index]]; index++;
      dstData[index] = values[srcData[index]]; index++;
      dstData[index] = 0xFF; index++;
    }
  }
};

