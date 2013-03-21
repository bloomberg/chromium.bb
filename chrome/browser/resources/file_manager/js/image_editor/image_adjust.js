// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The base class for simple filters that only modify the image content
 * but do not modify the image dimensions.
 * @constructor
 * @extends ImageEditor.Mode
 */
ImageEditor.Mode.Adjust = function() {
  ImageEditor.Mode.apply(this, arguments);
  this.implicitCommit = true;
  this.doneMessage_ = null;
  this.viewportGeneration_ = 0;
};

ImageEditor.Mode.Adjust.prototype = {__proto__: ImageEditor.Mode.prototype};

/** @override */
ImageEditor.Mode.Adjust.prototype.getCommand = function() {
  if (!this.filter_) return null;

  return new Command.Filter(this.name, this.filter_, this.doneMessage_);
};

/** @override */
ImageEditor.Mode.Adjust.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  this.hidePreview();
};

/**
 * TODO(JSDOC)
 */
ImageEditor.Mode.Adjust.prototype.hidePreview = function() {
  if (this.canvas_) {
    this.canvas_.parentNode.removeChild(this.canvas_);
    this.canvas_ = null;
  }
};

/**
 * TODO(JSDOC)
 */
ImageEditor.Mode.Adjust.prototype.cleanUpCaches = function() {
  this.filter_ = null;
  this.previewImageData_ = null;
};

/**
 * TODO(JSDOC)
 */
ImageEditor.Mode.Adjust.prototype.reset = function() {
  ImageEditor.Mode.prototype.reset.call(this);
  this.hidePreview();
  this.cleanUpCaches();
};

/**
 * TODO(JSDOC)
 * @param {Object} options  // TODO(JSDOC).
 */
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
      this.canvas_ = this.getImageView().createOverlayCanvas();
    }

    this.getImageView().setupDeviceBuffer(this.canvas_);

    this.originalImageData = this.getImageView().copyScreenImageData();
    this.previewImageData_ = this.getImageView().copyScreenImageData();
  }
};

/*
 * Own methods
 */

/**
 * TODO(JSDOC)
 * @param {Object} options  // TODO(JSDOC).
 * @return {function(ImageData,ImageData,number,number)} Created function.
 */
ImageEditor.Mode.Adjust.prototype.createFilter = function(options) {
  return filter.create(this.name, options);
};

/**
 * A base class for color filters that are scale independent.
 * @constructor
 */
ImageEditor.Mode.ColorFilter = function() {
  ImageEditor.Mode.Adjust.apply(this, arguments);
};

ImageEditor.Mode.ColorFilter.prototype =
    {__proto__: ImageEditor.Mode.Adjust.prototype};

/**
 * TODO(JSDOC)
 * @return {{r: Array.<number>, g: Array.<number>, b: Array.<number>}}
 *    histogram.
 */
ImageEditor.Mode.ColorFilter.prototype.getHistogram = function() {
  return filter.getHistogram(this.getImageView().getThumbnail());
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

/**
 * TODO(JSDOC)
 * @param {ImageEditor.Toolbar} toolbar The toolbar to populate.
 */
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

/**
 * TODO(JSDOC)
 * @param {ImageEditor.Toolbar} toolbar The toolbar to populate.
 */
ImageEditor.Mode.Autofix.prototype.createTools = function(toolbar) {
  var self = this;
  toolbar.addButton('Apply', this.apply.bind(this));
};

/**
 * TODO(JSDOC)
 * @return {boolean}  // TODO(JSDOC).
 */
ImageEditor.Mode.Autofix.prototype.isApplicable = function() {
  return this.getImageView().hasValidImage() &&
      filter.autofix.isApplicable(this.getHistogram());
};

/**
 * TODO(JSDOC)
 */
ImageEditor.Mode.Autofix.prototype.apply = function() {
  this.update({histogram: this.getHistogram()});
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

/**
 * TODO(JSDOC)
 */
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

/**
 * TODO(JSDOC)
 * @param {ImageEditor.Toolbar} toolbar The toolbar to populate.
 */
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

/**
 * TODO(JSDOC)
 * @param {ImageEditor.Toolbar} toolbar The toolbar to populate.
 */
ImageEditor.Mode.Sharpen.prototype.createTools = function(toolbar) {
  toolbar.addRange('strength', 0, 0, 1, 100);
  toolbar.addRange('radius', 1, 1, 3);
};
