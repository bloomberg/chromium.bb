// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The base class for simple filters that only modify the image content
 * but do not modify the image dimensions.
 * @constructor
 */
ImageEditor.Mode.Adjust = function(arglist) {
  ImageEditor.Mode.apply(this, arguments);
  this.viewportGeneration_ = 0;
};

ImageEditor.Mode.Adjust.prototype = {__proto__: ImageEditor.Mode.prototype};

/*
 *  ImageEditor.Mode methods overridden.
 */

ImageEditor.Mode.Adjust.prototype.commit = function() {
  if (!this.filter_) return; // Did not do anything yet.

  // Applying the filter to the entire image takes some time, so we do
  // it in small increments, providing visual feedback.
  // TODO: provide modal progress indicator.

  // First hide the preview and show the original image.
  this.repaint();

  var self = this;

  function repaintStrip(fromRow, toRow) {
    var imageStrip = new Rect(self.getViewport().getImageBounds());
    imageStrip.top = fromRow;
    imageStrip.height = toRow - fromRow;

    var screenStrip = new Rect(self.getViewport().getImageBoundsOnScreen());
    screenStrip.top = Math.round(self.getViewport().imageToScreenY(fromRow));
    screenStrip.height = Math.round(self.getViewport().imageToScreenY(toRow)) -
        screenStrip.top;

    self.getBuffer().repaintScreenRect(screenStrip, imageStrip);
  }

  ImageUtil.trace.resetTimer('filter');

  var lastUpdatedRow = 0;

  filter.applyByStrips(
      this.getContent().getCanvas().getContext('2d'),
      this.filter_,
      function (updatedRow, rowCount) {
        repaintStrip(lastUpdatedRow, updatedRow);
        lastUpdatedRow = updatedRow;
        if (updatedRow == rowCount) {
          ImageUtil.trace.reportTimer('filter');
          self.getContent().invalidateCaches();
          self.repaint();
        }
      });
};

ImageEditor.Mode.Adjust.prototype.cleanUpCaches = function() {
  this.filter_ = null;
  this.previewImageData_ = null;
};

ImageEditor.Mode.Adjust.prototype.update = function(options) {
  // We assume filter names are used in the UI directly.
  // This will have to change with i18n.
  this.filter_ = this.createFilter(options);
  this.previewValid_ = false;
  this.repaint();
};

/**
 * Clip and scale the source image data for the preview.
 * Use the cached copy if the viewport has not changed.
 */
ImageEditor.Mode.Adjust.prototype.updatePreviewImage = function() {
  if (!this.previewImageData_ ||
      this.viewportGeneration_ != this.getViewport().getCacheGeneration()) {
    this.viewportGeneration_ = this.getViewport().getCacheGeneration();

    var imageRect = this.getPreviewRect(this.getViewport().getImageClipped());
    var screenRect = this.getPreviewRect(this.getViewport().getScreenClipped());

    // Copy the visible part of the image at the current screen scale.
    var canvas = this.getContent().createBlankCanvas(
        screenRect.width, screenRect.height);
    var context = canvas.getContext('2d');
    Rect.drawImage(context, this.getContent().getCanvas(), null, imageRect);
    this.originalImageData =
        context.getImageData(0, 0, screenRect.width, screenRect.height);
    this.previewImageData_ =
        context.getImageData(0, 0, screenRect.width, screenRect.height);
    this.previewValid_ = false;
  }

  if (this.filter_ && !this.previewValid_) {
    ImageUtil.trace.resetTimer('preview');
    this.filter_(this.previewImageData_, this.originalImageData, 0, 0);
    ImageUtil.trace.reportTimer('preview');
    this.previewValid_ = true;
  }
};

ImageEditor.Mode.Adjust.prototype.draw = function(context) {
  this.updatePreviewImage();

  var screenClipped = this.getViewport().getScreenClipped();

  var previewRect = this.getPreviewRect(screenClipped);
  context.putImageData(
      this.previewImageData_, previewRect.left,  previewRect.top);

  if (previewRect.width < screenClipped.width &&
      previewRect.height < screenClipped.height) {
    // Some part of the original image is not covered by the preview,
    // shade it out.
    context.globalAlpha = 0.75;
    context.fillStyle = '#000000';
    context.strokeStyle = '#000000';
    Rect.fillBetween(
        context, previewRect, this.getViewport().getScreenBounds());
    Rect.outline(context, previewRect);
  }
};

/*
 * Own methods
 */

ImageEditor.Mode.Adjust.prototype.createFilter = function(options) {
  return filter.create(this.name, options);
};

ImageEditor.Mode.Adjust.prototype.getPreviewRect = function(rect) {
  if (this.getViewport().getScale() >= 1) {
    return rect;
  } else {
    var bounds = this.getViewport().getImageBounds();
    var screen = this.getViewport().getScreenClipped();

    screen = screen.inflate(-screen.width / 8, -screen.height / 8);

    return rect.inflate(-rect.width / 2, -rect.height / 2).
        inflate(Math.min(screen.width, bounds.width) / 2,
                Math.min(screen.height, bounds.height) / 2);
  }
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
  this.histogram_ =
      new ImageEditor.Mode.Histogram(this.getViewport(), this.getContent());
};

ImageEditor.Mode.ColorFilter.prototype.getPreviewRect = function(rect) {
  return rect;
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
ImageEditor.Mode.Histogram = function(viewport, content) {
  this.viewport_ = viewport;

  var canvas = content.getCanvas();
  var downScale = Math.max(1, Math.sqrt(canvas.width * canvas.height / 10000));
  var thumbnail = content.copyCanvas(canvas.width / downScale,
                                     canvas.height / downScale);
  var context = thumbnail.getContext('2d');

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

ImageEditor.Mode.register(ImageEditor.Mode.Exposure);

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
};

ImageEditor.Mode.Autofix.prototype =
    {__proto__: ImageEditor.Mode.ColorFilter.prototype};

ImageEditor.Mode.register(ImageEditor.Mode.Autofix);

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
};

ImageEditor.Mode.InstantAutofix.prototype =
    {__proto__: ImageEditor.Mode.Autofix.prototype};

ImageEditor.Mode.InstantAutofix.prototype.oneClick = function() {
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

// TODO(dgozman): register Mode.Blur in v2.

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

// TODO(dgozman): register Mode.Sharpen in v2.

ImageEditor.Mode.Sharpen.prototype.createTools = function(toolbar) {
  toolbar.addRange('strength', 0, 0, 1, 100);
  toolbar.addRange('radius', 1, 1, 3);
};
