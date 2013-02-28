// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ImageLoader = ImageLoader || {};

/**
 * Image loader's extension id.
 * @const
 * @type {string}
 */
ImageLoader.EXTENSION_ID = 'pmfjbimdmchhbnneeidfognadeopoehp';

/**
 * Client used to connect to the remote ImageLoader extension. Client class runs
 * in the extension, where the client.js is included (eg. Files.app).
 * It sends remote requests using IPC to the ImageLoader class and forwards
 * its responses.
 *
 * Implements cache, which is stored in the calling extension.
 *
 * @constructor
 */
ImageLoader.Client = function() {
  /**
   * @type {Port}
   * @private
   */
  this.port_ = chrome.extension.connect(ImageLoader.EXTENSION_ID);
  this.port_.onMessage.addListener(this.handleMessage_.bind(this));

  /**
   * Hash array with active tasks.
   * @type {Object}
   * @private
   */
  this.tasks_ = {};

  /**
   * @type {number}
   * @private
   */
  this.lastTaskId_ = 0;

  /**
   * LRU cache for images.
   * @type {ImageLoader.Client.Cache}
   * @private
   */
  this.cache_ = new ImageLoader.Client.Cache();
};

/**
 * Returns a singleton instance.
 * @return {ImageLoader.Client} ImageLoader.Client instance.
 */
ImageLoader.Client.getInstance = function() {
  if (!ImageLoader.Client.instance_)
    ImageLoader.Client.instance_ = new ImageLoader.Client();
  return ImageLoader.Client.instance_;
};

/**
 * Handles a message from the remote image loader and calls the registered
 * callback to pass the response back to the requester.
 *
 * @param {Object} message Response message as a hash array.
 * @private
 */
ImageLoader.Client.prototype.handleMessage_ = function(message) {
  if (!(message.taskId in this.tasks_)) {
    // This task has been canceled, but was already fetched, so it's result
    // should be discarded anyway.
    return;
  }

  var task = this.tasks_[message.taskId];

  // Check if the task is still valid.
  if (task.isValid())
    task.accept(message);

  delete this.tasks_[message.taskId];
};

/**
 * Loads and resizes and image. Use opt_isValid to easily cancel requests
 * which are not valid anymore, which will reduce cpu consumption.
 *
 * @param {string} url Url of the requested image.
 * @param {function} callback Callback used to return response.
 * @param {Object=} opt_options Loader options, such as: scale, maxHeight,
 *     width, height and/or cache.
 * @param {function=} opt_isValid Function returning false in case
 *     a request is not valid anymore, eg. parent node has been detached.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoader.Client.prototype.load = function(
    url, callback, opt_options, opt_isValid) {
  opt_options = opt_options || {};
  opt_isValid = opt_isValid || function() { return true; };

  // Cancel old, invalid tasks.
  var taskKeys = Object.keys(this.tasks_);
  for (var index = 0; index < taskKeys.length; index++) {
    var taskKey = taskKeys[index];
    var task = this.tasks_[taskKey];
    if (!task.isValid()) {
      // Cancel this task since it is not valid anymore.
      this.cancel(taskKey);
      delete this.tasks_[taskKey];
    }
  }

  // Replace the extension id.
  var sourceId = chrome.i18n.getMessage('@@extension_id');
  var targetId = ImageLoader.EXTENSION_ID;

  url = url.replace('filesystem:chrome-extension://' + sourceId,
                    'filesystem:chrome-extension://' + targetId);

  // Try to load from cache, if available.
  var cacheKey = ImageLoader.Client.Cache.createKey(url, opt_options);
  if (opt_options.cache) {
    // Load from cache.
    var cachedData = this.cache_.loadImage(cacheKey, opt_options.timestamp);
    if (cachedData) {
      callback({ status: 'success', data: cachedData });
      return null;
    }
  } else {
    // Remove from cache.
    this.cache_.removeImage(cacheKey);
  }

  // Not available in cache, performing a request to a remote extension.
  var request = opt_options;
  this.lastTaskId_++;
  var task = { isValid: opt_isValid, accept: function(result) {
    // Save to cache.
    if (result.status == 'success' && opt_options.cache)
      this.cache_.saveImage(cacheKey, result.data, opt_options.timestamp);
    callback(result);
  }.bind(this) };
  this.tasks_[this.lastTaskId_] = task;

  request.url = url;
  request.taskId = this.lastTaskId_;

  this.port_.postMessage(request);
  return request.taskId;
};

/**
 * Cancels the request.
 * @param {number} taskId Task id returned by ImageLoader.Client.load().
 */
ImageLoader.Client.prototype.cancel = function(taskId) {
  this.port_.postMessage({ taskId: taskId, cancel: true });
};

/**
 * Prints the cache usage statistics.
 */
ImageLoader.Client.prototype.stat = function() {
  this.cache_.stat();
};

/**
 * Least Recently Used (LRU) cache implementation to be used by
 * ImageLoader.Client class. It has memory constraints, so it will never
 * exceed specified memory limit defined in MEMORY_LIMIT.
 *
 * @constructor
 */
ImageLoader.Client.Cache = function() {
  this.images_ = [];
  this.size_ = 0;
};

/**
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
ImageLoader.Client.Cache.MEMORY_LIMIT = 100 * 1024 * 1024;  // 100 MB.

/**
 * Creates a cache key.
 *
 * @param {string} url Image url.
 * @param {Object=} opt_options Loader options as a hash array.
 * @return {string} Cache key.
 */
ImageLoader.Client.Cache.createKey = function(url, opt_options) {
  var array = opt_options || {};
  array.url = url;
  return JSON.stringify(array);
};

/**
 * Evicts the least used elements in cache to make space for a new image.
 *
 * @param {number} size Requested size.
 * @private
 */
ImageLoader.Client.Cache.prototype.evictCache_ = function(size) {
  // Sort from the most recent to the oldest.
  this.images_.sort(function(a, b) {
    return b.lastLoadTimestamp - a.lastLoadTimestamp;
  });

  while (this.images_.length > 0 &&
         (ImageLoader.Client.Cache.MEMORY_LIMIT - this.size_ < size)) {
    var entry = this.images_.pop();
    this.size_ -= entry.data.length;
  }
};

/**
 * Saves an image in the cache.
 *
 * @param {string} key Cache key.
 * @param {string} data Image data.
 * @param {number=} opt_timestamp Last modification timestamp. Used to detect
 *     if the cache entry becomes out of date.
 */
ImageLoader.Client.Cache.prototype.saveImage = function(
    key, data, opt_timestamp) {
  this.evictCache_(data.length);
  if (ImageLoader.Client.Cache.MEMORY_LIMIT - this.size_ >= data.length) {
    this.images_[key] = { lastLoadTimestamp: Date.now(),
                          timestamp: opt_timestamp ? opt_timestamp : null,
                          data: data };
    this.size_ += data.length;
  }
};

/**
 * Loads an image from the cache (if available) or returns null.
 *
 * @param {string} key Cache key.
 * @param {number=} opt_timestamp Last modification timestamp. If different
 *     that the one in cache, then the entry will be invalidated.
 * @return {?string} Data of the loaded image or null.
 */
ImageLoader.Client.Cache.prototype.loadImage = function(key, opt_timestamp) {
  if (!(key in this.images_))
    return null;

  var entry = this.images_[key];
  entry.lastLoadTimestamp = Date.now();

  // Check if the image in cache is up to date. If not, then remove it and
  // return null.
  if (entry.imageTimestamp === opt_timestamp) {
    this.removeImage(key);
    return null;
  }

  return entry.data;
};

/**
 * Prints the cache usage stats.
 */
ImageLoader.Client.Cache.prototype.stat = function() {
  console.log('Cache entries: ' + Object.keys(this.images_).length);
  console.log('Usage: ' + Math.round(this.size_ /
              ImageLoader.Client.Cache.MEMORY_LIMIT * 100.0) + '%');
};

/**
 * Removes the image from the cache.
 * @param {string} key Cache key.
 */
ImageLoader.Client.Cache.prototype.removeImage = function(key) {
  if (!(key in this.images_))
    return;

  var entry = this.images_[key];
  this.size_ -= entry.data.length;
  delete this.images_[key];
};

// Helper functions.

/**
 * Loads and resizes and image. Use opt_isValid to easily cancel requests
 * which are not valid anymore, which will reduce cpu consumption.
 *
 * @param {string} url Url of the requested image.
 * @param {Image} image Image node to load the requested picture into.
 * @param {Object} options Loader options, such as: scale, maxHeight, width,
 *     height and/or cache.
 * @param {function=} onSuccess Callback for success.
 * @param {function=} onError Callback for failure.
 * @param {function=} opt_isValid Function returning false in case
 *     a request is not valid anymore, eg. parent node has been detached.
 * @return {?number} Remote task id or null if loaded from cache.
 */
ImageLoader.Client.loadToImage = function(url, image, options, onSuccess,
    onError, opt_isValid) {
  var callback = function(result) {
    if (result.status == 'error') {
      onError();
      return;
    }
    image.src = result.data;
    onSuccess();
  };

  return ImageLoader.Client.getInstance().load(
      url, callback, options, opt_isValid);
};
