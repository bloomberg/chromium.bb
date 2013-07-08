// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Creates and starts downloading and then resizing of the image. Finally,
 * returns the image using the callback.
 *
 * @param {string} id Request ID.
 * @param {Cache} cache Cache object.
 * @param {Object} request Request message as a hash array.
 * @param {function} callback Callback used to send the response.
 * @constructor
 */
function Request(id, cache, request, callback) {
  /**
   * @type {string}
   * @private
   */
  this.id_ = id;

  /**
   * @type {Cache}
   * @private
   */
  this.cache_ = cache;

  /**
   * @type {Object}
   * @private
   */
  this.request_ = request;

  /**
   * @type {function}
   * @private
   */
  this.sendResponse_ = callback;

  /**
   * Temporary image used to download images.
   * @type {Image}
   * @private
   */
  this.image_ = new Image();

  /**
   * MIME type of the fetched image.
   * @type {string}
   * @private
   */
  this.contentType_ = null;

  /**
   * Used to download remote images using http:// or https:// protocols.
   * @type {XMLHttpRequest}
   * @private
   */
  this.xhr_ = new XMLHttpRequest();

  /**
   * Temporary canvas used to resize and compress the image.
   * @type {HTMLCanvasElement}
   * @private
   */
  this.canvas_ = document.createElement('canvas');

  /**
   * @type {CanvasRenderingContext2D}
   * @private
   */
  this.context_ = this.canvas_.getContext('2d');

  /**
   * Callback to be called once downloading is finished.
   * @type {function()}
   * @private
   */
  this.downloadCallback_ = null;
}

/**
 * Returns ID of the request.
 * @return {string} Request ID.
 */
Request.prototype.getId = function() {
  return this.id_;
};

/**
 * Returns priority of the request. The higher priority, the faster it will
 * be handled. The highest priority is 0. The default one is 2.
 *
 * @return {number} Priority.
 */
Request.prototype.getPriority = function() {
  return (this.request_.priority !== undefined) ? this.request_.priority : 2;
};

/**
 * Tries to load the image from cache if exists and sends the response.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 */
Request.prototype.loadFromCacheAndProcess = function(onSuccess, onFailure) {
  this.loadFromCache_(
      function(data) {  // Found in cache.
        this.sendImageData_(data);
        onSuccess();
      }.bind(this),
      onFailure);  // Not found in cache.
};

/**
 * Tries to download the image, resizes and sends the response.
 * @param {function()} callback Completion callback.
 */
Request.prototype.downloadAndProcess = function(callback) {
  if (this.downloadCallback_)
    throw new Error('Downloading already started.');

  this.downloadCallback_ = callback;
  this.downloadOriginal_(this.onImageLoad_.bind(this),
                         this.onImageError_.bind(this));
};

/**
 * Fetches the image from the persistent cache.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
Request.prototype.loadFromCache_ = function(onSuccess, onFailure) {
  var cacheKey = Cache.createKey(this.request_);

  if (!this.request_.cache) {
    // Cache is disabled for this request; therefore, remove it from cache
    // if existed.
    this.cache_.removeImage(cacheKey);
    onFailure();
    return;
  }

  if (!this.request_.timestamp) {
    // Persistent cache is available only when a timestamp is provided.
    onFailure();
    return;
  }

  this.cache_.loadImage(cacheKey,
                        this.request_.timestamp,
                        onSuccess,
                        onFailure);
};

/**
 * Saves the image to the persistent cache.
 *
 * @param {string} data The image's data.
 * @private
 */
Request.prototype.saveToCache_ = function(data) {
  if (!this.request_.cache || !this.request_.timestamp) {
    // Persistent cache is available only when a timestamp is provided.
    return;
  }

  var cacheKey = Cache.createKey(this.request_);
  this.cache_.saveImage(cacheKey,
                        data,
                        this.request_.timestamp);
};

/**
 * Downloads an image directly or for remote resources using the XmlHttpRequest.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
Request.prototype.downloadOriginal_ = function(onSuccess, onFailure) {
  this.image_.onload = onSuccess;
  this.image_.onerror = onFailure;

  // Download data urls directly since they are not supported by XmlHttpRequest.
  var dataUrlMatches = this.request_.url.match(/^data:([^,;]*)[,;]/);
  if (dataUrlMatches) {
    this.image_.src = this.request_.url;
    this.contentType_ = dataUrlMatches[1];
    return;
  }

  // Download using an xhr request.
  this.xhr_.responseType = 'blob';

  this.xhr_.onerror = this.image_.onerror;
  this.xhr_.onload = function() {
    if (this.xhr_.status != 200) {
      this.image_.onerror();
      return;
    }

    // Process returned data, including the mime type.
    this.contentType_ = this.xhr_.getResponseHeader('Content-Type');
    var reader = new FileReader();
    reader.onerror = this.image_.onerror;
    reader.onload = function(e) {
      this.image_.src = e.target.result;
    }.bind(this);

    // Load the data to the image as a data url.
    reader.readAsDataURL(this.xhr_.response);
  }.bind(this);

  // Perform a xhr request.
  try {
    this.xhr_.open('GET', this.request_.url, true);
    this.xhr_.send();
  } catch (e) {
    this.image_.onerror();
  }
};

/**
 * Sends the resized image via the callback. If the image has been changed,
 * then packs the canvas contents, otherwise sends the raw image data.
 *
 * @param {boolean} imageChanged Whether the image has been changed.
 * @private
 */
Request.prototype.sendImage_ = function(imageChanged) {
  var imageData;
  if (!imageChanged) {
    // The image hasn't been processed, so the raw data can be directly
    // forwarded for speed (no need to encode the image again).
    imageData = this.image_.src;
  } else {
    // The image has been resized or rotated, therefore the canvas has to be
    // encoded to get the correct compressed image data.
    switch (this.contentType_) {
      case 'image/gif':
      case 'image/png':
      case 'image/svg':
      case 'image/bmp':
        imageData = this.canvas_.toDataURL('image/png');
        break;
      case 'image/jpeg':
      default:
        imageData = this.canvas_.toDataURL('image/jpeg', 0.9);
    }
  }

  // Send and store in the persistent cache.
  this.sendImageData_(imageData);
  this.saveToCache_(imageData);
};

/**
 * Sends the resized image via the callback.
 * @param {string} data Compressed image data.
 * @private
 */
Request.prototype.sendImageData_ = function(data) {
  this.sendResponse_({status: 'success',
                      data: data,
                      taskId: this.request_.taskId});
};

/**
 * Handler, when contents are loaded into the image element. Performs resizing
 * and finalizes the request process.
 *
 * @param {function()} callback Completion callback.
 * @private
 */
Request.prototype.onImageLoad_ = function(callback) {
  // Perform processing if the url is not a data url, or if there are some
  // operations requested.
  if (!this.request_.url.match(/^data/) ||
      ImageLoader.shouldProcess(this.image_.width,
                                this.image_.height,
                                this.request_)) {
    ImageLoader.resize(this.image_, this.canvas_, this.request_);
    this.sendImage_(true);  // Image changed.
  } else {
    this.sendImage_(false);  // Image not changed.
  }
  this.cleanup_();
  this.downloadCallback_();
};

/**
 * Handler, when loading of the image fails. Sends a failure response and
 * finalizes the request process.
 *
 * @param {function()} callback Completion callback.
 * @private
 */
Request.prototype.onImageError_ = function(callback) {
  this.sendResponse_({status: 'error',
                      taskId: this.request_.taskId});
  this.cleanup_();
  this.downloadCallback_();
};

/**
 * Cancels the request.
 */
Request.prototype.cancel = function() {
  this.cleanup_();

  // If downloading has started, then call the callback.
  if (this.downloadCallback_)
    this.downloadCallback_();
};

/**
 * Cleans up memory used by this request.
 * @private
 */
Request.prototype.cleanup_ = function() {
  this.image_.onerror = function() {};
  this.image_.onload = function() {};

  // Transparent 1x1 pixel gif, to force garbage collecting.
  this.image_.src = 'data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAA' +
      'ABAAEAAAICTAEAOw==';

  this.xhr_.onerror = function() {};
  this.xhr_.onload = function() {};
  this.xhr_.abort();

  // Dispose memory allocated by Canvas.
  this.canvas_.width = 0;
  this.canvas_.height = 0;
};
