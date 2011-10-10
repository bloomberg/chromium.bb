// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The base class for simple filters that only modify the image content
 * but do not modify the image dimensions.
 * @constructor
 */
ImageEditor.Mode.Adjust = function() {
  ImageEditor.Mode.apply(this, arguments);
  this.implicitCommit = true;
  this.doneMessage_ = null;
  this.viewportGeneration_ = 0;
};

ImageEditor.Mode.Adjust.prototype = {__proto__: ImageEditor.Mode.prototype};

/*
 *  ImageEditor.Mode methods overridden.
 */

ImageEditor.Mode.Adjust.prototype.getCommand = function() {
  if (!this.filter_) return null;

  return new Command.Filter(this.name, this.filter_, this.doneMessage_);
};

ImageEditor.Mode.Adjust.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  if (this.canvas_) {
    this.canvas_.parentNode.removeChild(this.canvas_);
    this.canvas_ = null;
  }
};

ImageEditor.Mode.Adjust.prototype.cleanUpCaches = function() {
  this.filter_ = null;
  this.previewImageData_ = null;
};

ImageEditor.Mode.Adjust.prototype.update = function(options) {
  ImageEditor.Mode.prototype.update.apply(this, arguments);

  // We assume filter names are used in the UI directly.
  // This will have to change with i18n.
  this.filter_ = this.createFilter(options);
  this.updatePreviewImage();
  ImageUtil.trace.resetTimer('preview');
  this.filter_(this.previewImageData_, this.originalImageData, 0, 0);
  ImageUtil.trace.reportTimer('preview');
  this.canvas_.getContext('2d').putImageData(
      this.previewImageData_, 0, 0);
};

/**
 * Copy the source image data for the preview.
 * Use the cached copy if the viewport has not changed.
 */
ImageEditor.Mode.Adjust.prototype.updatePreviewImage = function() {
  if (!this.previewImageData_ ||
      this.viewportGeneration_ != this.getViewport().getCacheGeneration()) {
    this.viewportGeneration_ = this.getViewport().getCacheGeneration();

    if (!this.canvas_) {
      var container = this.getImageView().container_;
      this.canvas_ = container.ownerDocument.createElement('canvas');
      container.appendChild(this.canvas_);
    }

    var screenClipped = this.getViewport().getScreenClipped();

    this.canvas_.style.left = screenClipped.left + 'px';
    this.canvas_.style.top = screenClipped.top + 'px';
    if (this.canvas_.width != screenClipped.width)
      this.canvas_.width = screenClipped.width;
    if (this.canvas_.height != screenClipped.height)
      this.canvas_.height = screenClipped.height;

    this.originalImageData = this.getImageView().copyScreenImageData();
    this.previewImageData_ = this.getImageView().copyScreenImageData();
  }
};

/*
 * Own methods
 */

ImageEditor.Mode.Adjust.prototype.createFilter = function(options) {
  return filter.create(this.name, options);
};

/**
 * A base class for color filters that are scale independent (i.e. can
 * be applied to a scaled image with basicaly the same effect).
 * Displays a histogram.
 * @constructor
 */
ImageEditor.Mode.ColorFilter = function() {
  ImageEditor.Mode.Adjust.apply(this, arguments);
};

ImageEditor.Mode.ColorFilter.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

ImageEditor.Mode.ColorFilter.prototype.setUp = function() {
  ImageEditor.Mode.Adjust.prototype.setUp.apply(this, arguments);
  this.histogram_ = new ImageEditor.Mode.Histogram(
      this.getViewport(), this.getImageView().getCanvas());
};

ImageEditor.Mode.ColorFilter.prototype.createFilter = function(options) {
  var filterFunc =
      ImageEditor.Mode.Adjust.prototype.createFilter.apply(this, arguments);
  this.histogram_.update(filterFunc);
  return filterFunc;
};

ImageEditor.Mode.ColorFilter.prototype.cleanUpUI = function() {
  ImageEditor.Mode.Adjust.prototype.cleanUpUI.apply(this, arguments);
  this.histogram_ = null;
};

/**
 * A histogram container.
 * @constructor
 */
ImageEditor.Mode.Histogram = function(viewport, canvas) {
  this.viewport_ = viewport;

  var downScale = Math.max(1, Math.sqrt(canvas.width * canvas.height / 10000));

  var thumbnail = canvas.ownerDocument.createElement('canvas');
  thumbnail.width = canvas.width / downScale;
  thumbnail.height = canvas.height / downScale;
  var context = thumbnail.getContext('2d');
  Rect.drawImage(context, canvas);

  this.originalImageData_ =
      context.getImageData(0, 0, thumbnail.width, thumbnail.height);
  this.filteredImageData_ =
      context.getImageData(0, 0, thumbnail.width, thumbnail.height);

  this.update();
};

ImageEditor.Mode.Histogram.prototype.getData = function() { return this.data_ };

ImageEditor.Mode.Histogram.BUCKET_WIDTH = 8;
ImageEditor.Mode.Histogram.BAR_WIDTH = 2;
ImageEditor.Mode.Histogram.RIGHT = 5;
ImageEditor.Mode.Histogram.TOP = 5;

ImageEditor.Mode.Histogram.prototype.update = function(filterFunc) {
  if (filterFunc) {
    filterFunc(this.filteredImageData_, this.originalImageData_, 0, 0);
    this.data_ = filter.getHistogram(this.filteredImageData_);
  } else {
    this.data_ = filter.getHistogram(this.originalImageData_);
  }
};

ImageEditor.Mode.Histogram.prototype.draw = function(context) {
  var screen = this.viewport_.getScreenBounds();

  var barCount = 2 + 3 * (256 / ImageEditor.Mode.Histogram.BUCKET_WIDTH);
  var width = ImageEditor.Mode.Histogram.BAR_WIDTH * barCount;
  var height = Math.round(width / 2);
  var rect = new Rect(
      screen.left + screen.width - ImageEditor.Mode.Histogram.RIGHT - width,
      ImageEditor.Mode.Histogram.TOP,
      width,
      height);

  context.globalAlpha = 1;
  context.fillStyle = '#E0E0E0';
  context.strokeStyle = '#000000';
  context.lineCap = 'square';
  Rect.fill(context, rect);
  Rect.outline(context, rect);

  function drawChannel(channel, style, offsetX, offsetY) {
    context.strokeStyle = style;
    context.beginPath();
    for (var i  = 0; i != 256; i += ImageEditor.Mode.Histogram.BUCKET_WIDTH) {
      var barHeight = channel[i];
      for (var b = 1; b < ImageEditor.Mode.Histogram.BUCKET_WIDTH; b++)
        barHeight = Math.max(barHeight, channel[i + b]);
      barHeight = Math.min(barHeight, height);
      for (var j = 0; j != ImageEditor.Mode.Histogram.BAR_WIDTH; j++) {
        context.moveTo(offsetX, offsetY);
        context.lineTo(offsetX, offsetY - barHeight);
        offsetX++;
      }
      offsetX += 2 * ImageEditor.Mode.Histogram.BAR_WIDTH;
    }
    context.closePath();
    context.stroke();
  }

  var offsetX = rect.left + 0.5 + ImageEditor.Mode.Histogram.BAR_WIDTH;
  var offsetY = rect.top + rect.height;

  drawChannel(this.data_.r, '#F00000', offsetX, offsetY);
  offsetX += ImageEditor.Mode.Histogram.BAR_WIDTH;
  drawChannel(this.data_.g, '#00F000', offsetX, offsetY);
  offsetX += ImageEditor.Mode.Histogram.BAR_WIDTH;
  drawChannel(this.data_.b, '#0000F0', offsetX, offsetY);
};

/**
 * Exposure/contrast filter.
 * @constructor
 */
ImageEditor.Mode.Exposure = function() {
  ImageEditor.Mode.ColorFilter.call(this, 'exposure');
};

ImageEditor.Mode.Exposure.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

ImageEditor.Mode.Exposure.prototype.createTools = function(toolbar) {
  toolbar.addRange('brightness', -1, 0, 1, 100);
  toolbar.addRange('contrast', -1, 0, 1, 100);
};

/**
 * Autofix.
 * @constructor
 */
ImageEditor.Mode.Autofix = function() {
  ImageEditor.Mode.ColorFilter.call(this, 'autofix');
  this.doneMessage_ = 'fixed';
};

ImageEditor.Mode.Autofix.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

ImageEditor.Mode.Autofix.prototype.createTools = function(toolbar) {
  var self = this;
  toolbar.addButton('Apply', this.apply.bind(this));
};

ImageEditor.Mode.Autofix.prototype.apply = function() {
  this.update({histogram: this.histogram_.getData()});
};

/**
 * Instant Autofix.
 * @constructor
 */
ImageEditor.Mode.InstantAutofix = function() {
  ImageEditor.Mode.Autofix.apply(this, arguments);
  this.instant = true;
};

ImageEditor.Mode.InstantAutofix.prototype =
    {__proto__: ImageEditor.Mode.Autofix.prototype};

ImageEditor.Mode.InstantAutofix.prototype.setUp = function() {
  ImageEditor.Mode.Autofix.prototype.setUp.apply(this, arguments);
  this.apply();
};

/**
 * Blur filter.
 * @constructor
 */
ImageEditor.Mode.Blur = function() {
  ImageEditor.Mode.Adjust.call(this, 'blur');
};

ImageEditor.Mode.Blur.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

ImageEditor.Mode.Blur.prototype.createTools = function(toolbar) {
  toolbar.addRange('strength', 0, 0, 1, 100);
  toolbar.addRange('radius', 1, 1, 3);
};

/**
 * Sharpen filter.
 * @constructor
 */
ImageEditor.Mode.Sharpen = function() {
  ImageEditor.Mode.Adjust.call(this, 'sharpen');
};

ImageEditor.Mode.Sharpen.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

ImageEditor.Mode.Sharpen.prototype.createTools = function(toolbar) {
  toolbar.addRange('strength', 0, 0, 1, 100);
  toolbar.addRange('radius', 1, 1, 3);
};
