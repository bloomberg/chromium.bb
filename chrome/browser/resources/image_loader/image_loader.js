// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Loads and resizes an image.
 * @constructor
 */
var ImageLoader = function() {
  /**
   * Hash array of active requests.
   * @type {Object}
   * @private
   */
  this.requests_ = {};

  chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
    // TODO(mtomasz): Handle.
  });

  chrome.extension.onConnectExternal.addListener(function(port) {
    if (ImageLoader.ALLOWED_CLIENTS.indexOf(port.sender.id) !== -1)
      port.onMessage.addListener(function(request) {
        this.onMessage_(port, request, port.postMessage.bind(port));
      }.bind(this));
  }.bind(this));
};

/**
 * List of extensions allowed to perform image requests.
 *
 * @const
 * @type {Array.<string>}
 */
ImageLoader.ALLOWED_CLIENTS =
    ['hhaomjibdihmijegdhdafkllkbggdgoj'];  // File Manager's extension id.

/**
 * Handles a request. Depending on type of the request, starts or stops
 * an image task.
 *
 * @param {Port} port Connection port.
 * @param {Object} request Request message as a hash array.
 * @param {function} callback Callback to be called to return response.
 * @private
 */
ImageLoader.prototype.onMessage_ = function(port, request, callback) {
  var requestId = port.sender.id + ':' + request.taskId;
  if (request.cancel) {
    // Cancel a task.
    if (requestId in this.requests_) {
      this.requests_[requestId].cancel();
      delete this.requests_[requestId];
    }
  } else {
    // Start a task.
    this.requests_[requestId] =
        new ImageLoader.Request(request, callback);
  }
};

/**
 * Returns the singleton instance.
 * @return {ImageLoader} ImageLoader object.
 */
ImageLoader.getInstance = function() {
  if (!ImageLoader.instance_)
    ImageLoader.instance_ = new ImageLoader();
  return ImageLoader.instance_;
};

/**
 * Calculates dimensions taking into account resize options, such as:
 * - scale: for scaling,
 * - maxWidth, maxHeight: for maximum dimensions,
 * - width, height: for exact requested size.
 * Returns the target size as hash array with width, height properties.
 *
 * @param {number} width Source width.
 * @param {number} height Source height.
 * @param {Object} options Resizing options as a hash array.
 * @return {Object} Dimensions, eg. { width: 100, height: 50 }.
 */
ImageLoader.resizeDimensions = function(width, height, options) {
  var sourceWidth = width;
  var sourceHeight = height;

  var targetWidth = sourceWidth;
  var targetHeight = sourceHeight;

  if ('scale' in options) {
    targetWidth = sourceWidth * options.scale;
    targetHeight = sourceHeight * options.scale;
  }

  if (options.maxWidth &&
      targetWidth > options.maxWidth) {
      var scale = options.maxWidth / targetWidth;
      targetWidth *= scale;
      targetHeight *= scale;
  }

  if (options.maxHeight &&
      targetHeight > options.maxHeight) {
      var scale = options.maxHeight / targetHeight;
      targetWidth *= scale;
      targetHeight *= scale;
  }

  if (options.width)
    targetWidth = options.width;

  if (options.height)
    targetHeight = options.height;

  targetWidth = Math.round(targetWidth);
  targetHeight = Math.round(targetHeight);

  return { width: targetWidth, height: targetHeight };
};

/**
 * Performs resizing of the source image into the target canvas.
 *
 * @param {HTMLCanvasElement|Image} source Source image or canvas.
 * @param {HTMLCanvasElement} target Target canvas.
 * @param {Object} options Resizing options as a hash array.
 */
ImageLoader.resize = function(source, target, options) {
  var targetDimensions = ImageLoader.resizeDimensions(
      source.width, source.height, options);

  target.width = targetDimensions.width;
  target.height = targetDimensions.height;

  var targetContext = target.getContext('2d');
  targetContext.drawImage(source,
                          0, 0, source.width, source.height,
                          0, 0, target.width, target.height);
};

/**
 * Creates and starts downloading and then resizing of the image. Finally,
 * returns the image using the callback.
 *
 * @param {Object} request Request message as a hash array.
 * @param {function} callback Callback used to send the response.
 * @constructor
 */
ImageLoader.Request = function(request, callback) {
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

  this.downloadOriginal_();
};

/**
 * Downloads an image directly or for remote resources using the XmlHttpRequest.
 * @private
 */
ImageLoader.Request.prototype.downloadOriginal_ = function() {
  this.image_.onload = this.onImageLoad_.bind(this);
  this.image_.onerror = this.onImageError_.bind(this);

  if (window.harness || !this.request_.url.match(/^https?:/)) {
    // Download directly.
    this.image_.src = this.request_.url;
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

    // Process returnes data.
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
 * Sends the resized image via the callback.
 * @private
 */
ImageLoader.Request.prototype.sendImage_ = function() {
  // TODO(mtomasz): Keep format. Never compress using jpeg codec for lossless
  // images such as png, gif.
  var pngData = this.canvas_.toDataURL('image/png');
  var jpegData = this.canvas_.toDataURL('image/jpeg', 0.9);
  var imageData = pngData.length < jpegData.length * 2 ? pngData : jpegData;
  this.sendResponse_({ status: 'success',
                       data: imageData,
                       taskId: this.request_.taskId });
};

/**
 * Handler, when contents are loaded into the image element. Performs resizing
 * and finalizes the request process.
 *
 * @private
 */
ImageLoader.Request.prototype.onImageLoad_ = function() {
  ImageLoader.resize(this.image_, this.canvas_, this.request_);
  this.sendImage_();
  this.cleanup_();
};

/**
 * Handler, when loading of the image fails. Sends a failure response and
 * finalizes the request process.
 *
 * @private
 */
ImageLoader.Request.prototype.onImageError_ = function() {
  this.sendResponse_({ status: 'error',
                       taskId: this.request_.taskId });
  this.cleanup_();
};

/**
 * Cancels the request.
 */
ImageLoader.Request.prototype.cancel = function() {
  this.cleanup_();
};

/**
 * Cleans up memory used by this request.
 * @private
 */
ImageLoader.Request.prototype.cleanup_ = function() {
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

// Load the extension.
ImageLoader.getInstance();
