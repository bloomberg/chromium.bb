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

  /**
   * Persistent cache object.
   * @type {ImageLoader.Cache}
   * @private
   */
  this.cache_ = new ImageLoader.Cache();

  chrome.fileBrowserPrivate.requestLocalFileSystem(function(filesystem) {
    // TODO(mtomasz): Handle.
  });

  chrome.extension.onMessageExternal.addListener(function(request,
                                                          sender,
                                                          sendResponse) {
    if (ImageLoader.ALLOWED_CLIENTS.indexOf(sender.id) !== -1)
      return this.onMessage_(sender.id, request, sendResponse);
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
 * @param {string} senderId Sender's extension id.
 * @param {Object} request Request message as a hash array.
 * @param {function} callback Callback to be called to return response.
 * @return {boolean} True if the message channel should stay alive until the
 *     callback is called.
 * @private
 */
ImageLoader.prototype.onMessage_ = function(senderId, request, callback) {
  var requestId = senderId + ':' + request.taskId;
  if (request.cancel) {
    // Cancel a task.
    if (requestId in this.requests_) {
      this.requests_[requestId].cancel();
      delete this.requests_[requestId];
    }
    return false;  // No callback calls.
  } else {
    // Start a task.
    this.requests_[requestId] =
        new ImageLoader.Request(this.cache_, request, callback);
    return true;  // Request will call the callback.
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
 * @return {Object} Dimensions, eg. {width: 100, height: 50}.
 */
ImageLoader.resizeDimensions = function(width, height, options) {
  var sourceWidth = width;
  var sourceHeight = height;

  // Flip dimensions for odd orientation values: 1 (90deg) and 3 (270deg).
  if (options.orientation && options.orientation % 2) {
    sourceWidth = height;
    sourceHeight = width;
  }

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

  return {width: targetWidth, height: targetHeight};
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

  // Default orientation is 0deg.
  var orientation = options.orientation || 0;

  // For odd orientation values: 1 (90deg) and 3 (270deg) flip dimensions.
  if (orientation % 2) {
    drawImageWidth = target.height;
    drawImageHeight = target.width;
  } else {
    drawImageWidth = target.width;
    drawImageHeight = target.height;
  }

  var targetContext = target.getContext('2d');
  targetContext.save();
  targetContext.translate(target.width / 2, target.height / 2);
  targetContext.rotate(orientation * Math.PI / 2);
  targetContext.drawImage(
      source,
      0, 0,
      source.width, source.height,
      -drawImageWidth / 2, -drawImageHeight / 2,
      drawImageWidth, drawImageHeight);
  targetContext.restore();
};

/**
 * Creates and starts downloading and then resizing of the image. Finally,
 * returns the image using the callback.
 *
 * @param {ImageLoader.Cache} cache Cache object.
 * @param {Object} request Request message as a hash array.
 * @param {function} callback Callback used to send the response.
 * @constructor
 */
ImageLoader.Request = function(cache, request, callback) {
  /**
   * @type {ImageLoader.Cache}
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

  // Process the request. Try to load from cache. If it fails, then download.
  this.loadFromCache_(
      this.sendImageData_.bind(this),
      function() {  // Failure, not in cache.
        this.downloadOriginal_(this.onImageLoad_.bind(this),
                               this.onImageError_.bind(this));
      }.bind(this));
};

/**
 * Fetches the image from the persistent cache.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
ImageLoader.Request.prototype.loadFromCache_ = function(onSuccess, onFailure) {
  var cacheKey = ImageLoader.Cache.createKey(this.request_);

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
 * @param {string} data The image's data.
 * @private
 */
ImageLoader.Request.prototype.saveToCache_ = function(data) {
  if (!this.request_.cache || !this.request_.timestamp) {
    // Persistent cache is available only when a timestamp is provided.
    return;
  }

  var cacheKey = ImageLoader.Cache.createKey(this.request_);
  this.cache_.saveImage(cacheKey,
                        data,
                        this.request_.timestamp);
};

/**
 * Downloads an image directly or for remote resources using the XmlHttpRequest.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
ImageLoader.Request.prototype.downloadOriginal_ = function(
    onSuccess, onFailure) {
  this.image_.onload = onSuccess;
  this.image_.onerror = onFailure;

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

  // Send and store in the persistent cache.
  this.sendImageData_(imageData);
  this.saveToCache_(imageData);
};

/**
 * Sends the resized image via the callback.
 * @param {string} data Compressed image data.
 * @private
 */
ImageLoader.Request.prototype.sendImageData_ = function(data) {
  this.sendResponse_({status: 'success',
                      data: data,
                      taskId: this.request_.taskId});
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
  this.sendResponse_({status: 'error',
                      taskId: this.request_.taskId});
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

/**
 * Persistent cache storing images in an indexed database on the hard disk.
 * @constructor
 */
ImageLoader.Cache = function() {
  /**
   * IndexedDB database handle.
   * @type {IDBDatabase}
   * @private
   */
  this.db_ = null;

  // Establish a connection to the database or (re)create it if not available
  // or not up to date. After changing the database's schema, increment
  // ImageLoader.Cache.DB_VERSION to force database recreating.
  var openRequest = window.webkitIndexedDB.open(ImageLoader.Cache.DB_NAME,
                                                ImageLoader.Cache.DB_VERSION);

  openRequest.onsuccess = function(e) {
    this.db_ = e.target.result;
  }.bind(this);

  openRequest.onupgradeneeded = function(e) {
    console.info('Cache database creating or upgrading.');
    var db = e.target.result;
    if (db.objectStoreNames.contains('metadata'))
      db.deleteObjectStore('metadata');
    if (db.objectStoreNames.contains('data'))
      db.deleteObjectStore('data');
    if (db.objectStoreNames.contains('settings'))
      db.deleteObjectStore('settings');
    db.createObjectStore('metadata', {keyPath: 'key'});
    db.createObjectStore('data', {keyPath: 'key'});
    db.createObjectStore('settings', {keyPath: 'key'});
  };
};

/**
 * Cache database name.
 * @type {string}
 * @const
 */
ImageLoader.Cache.DB_NAME = 'image-loader';

/**
 * Cache database version.
 * @type {number}
 * @const
 */
ImageLoader.Cache.DB_VERSION = 8;

/**
 * Memory limit for images data in bytes.
 *
 * @const
 * @type {number}
 */
ImageLoader.Cache.MEMORY_LIMIT = 250 * 1024 * 1024;  // 250 MB.

/**
 * Minimal amount of memory freed per eviction. Used to limit number of
 * evictions which are expensive.
 *
 * @const
 * @type {number}
 */
ImageLoader.Cache.EVICTION_CHUNK_SIZE = 50 * 1024 * 1024;  // 50 MB.

/**
 * Creates a cache key.
 *
 * @param {Object} request Request options.
 * @return {string} Cache key.
 */
ImageLoader.Cache.createKey = function(request) {
  return JSON.stringify({url: request.url,
                         scale: request.scale,
                         width: request.width,
                         height: request.height,
                         maxWidth: request.maxWidth,
                         maxHeight: request.maxHeight});
};

/**
 * Sets size of the cache.
 *
 * @param {number} size Size in bytes.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 * @private
 */
ImageLoader.Cache.prototype.setCacheSize_ = function(size, opt_transaction) {
  var transaction = opt_transaction ||
      this.db_.transaction(['settings'], 'readwrite');
  var settingsStore = transaction.objectStore('settings');

  settingsStore.put({key: 'size', value: size});  // Update asynchronously.
};

/**
 * Fetches current size of the cache.
 *
 * @param {function(number)} onSuccess Callback to return the size.
 * @param {function()} onFailure Failure callback.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 * @private
 */
ImageLoader.Cache.prototype.fetchCacheSize_ = function(
    onSuccess, onFailure, opt_transaction) {
  var transaction = opt_transaction ||
      this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
  var settingsStore = transaction.objectStore('settings');
  var sizeRequest = settingsStore.get('size');

  sizeRequest.onsuccess = function(e) {
    if (e.target.result)
      onSuccess(e.target.result.value);
    else
      onSuccess(0);
  };

  sizeRequest.onerror = function() {
    console.error('Failed to fetch size from the database.');
    onFailure();
  };
};

/**
 * Evicts the least used elements in cache to make space for a new image and
 * updates size of the cache taking into account the upcoming item.
 *
 * @param {number} size Requested size.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onFailure Failure callback.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 * @private
 */
ImageLoader.Cache.prototype.evictCache_ = function(
    size, onSuccess, onFailure, opt_transaction) {
  var transaction = opt_transaction ||
      this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');

  // Check if the requested size is smaller than the cache size.
  if (size > ImageLoader.Cache.MEMORY_LIMIT) {
    onFailure();
    return;
  }

  var onCacheSize = function(cacheSize) {
    if (size < ImageLoader.Cache.MEMORY_LIMIT - cacheSize) {
      // Enough space, no need to evict.
      this.setCacheSize_(cacheSize + size, transaction);
      onSuccess();
      return;
    }

    var bytesToEvict = Math.max(size,
                                ImageLoader.Cache.EVICTION_CHUNK_SIZE);

    // Fetch all metadata.
    var metadataEntries = [];
    var metadataStore = transaction.objectStore('metadata');
    var dataStore = transaction.objectStore('data');

    var onEntriesFetched = function() {
      metadataEntries.sort(function(a, b) {
        return b.lastLoadTimestamp - a.lastLoadTimestamp;
      });

      var totalEvicted = 0;
      while (bytesToEvict > 0) {
        var entry = metadataEntries.pop();
        totalEvicted += entry.size;
        bytesToEvict -= entry.size;
        metadataStore.delete(entry.key);  // Remove asynchronously.
        dataStore.delete(entry.key);  // Remove asynchronously.
      }

      this.setCacheSize_(cacheSize - totalEvicted + size, transaction);
    }.bind(this);

    metadataStore.openCursor().onsuccess = function(e) {
      var cursor = event.target.result;
      if (cursor) {
        metadataEntries.push(cursor.value);
        cursor.continue();
      } else {
        onEntriesFetched();
      }
    };
  }.bind(this);

  this.fetchCacheSize_(onCacheSize, onFailure, transaction);
};

/**
 * Saves an image in the cache.
 *
 * @param {string} key Cache key.
 * @param {string} data Image data.
 * @param {number} timestamp Last modification timestamp. Used to detect
 *     if the cache entry becomes out of date.
 */
ImageLoader.Cache.prototype.saveImage = function(key, data, timestamp) {
  if (!this.db_) {
    console.warn('Cache database not available.');
    return;
  }

  var onNotFoundInCache = function() {
    var metadataEntry = {key: key,
                         timestamp: timestamp,
                         size: data.length,
                         lastLoadTimestamp: Date.now()};
    var dataEntry = {key: key,
                     data: data};

    var transaction = this.db_.transaction(['settings', 'metadata', 'data'],
                                          'readwrite');
    var metadataStore = transaction.objectStore('metadata');
    var dataStore = transaction.objectStore('data');

    var onCacheEvicted = function() {
      metadataStore.put(metadataEntry);  // Add asynchronously.
      dataStore.put(dataEntry);  // Add asynchronously.
    };

    // Make sure there is enough space in the cache.
    this.evictCache_(data.length, onCacheEvicted, function() {}, transaction);
  }.bind(this);

  // Check if the image is already in cache. If not, then save it to cache.
  this.loadImage(key, timestamp, function() {}, onNotFoundInCache);
};

/**
 * Loads an image from the cache (if available) or returns null.
 *
 * @param {string} key Cache key.
 * @param {number} timestamp Last modification timestamp. If different
 *     that the one in cache, then the entry will be invalidated.
 * @param {function(<string>)} onSuccess Success callback with the image's data.
 * @param {function()} onFailure Failure callback.
 */
ImageLoader.Cache.prototype.loadImage = function(
    key, timestamp, onSuccess, onFailure) {
  if (!this.db_) {
    console.warn('Cache database not available.');
    onFailure();
    return;
  }

  var transaction = this.db_.transaction(['settings', 'metadata', 'data'],
                                         'readwrite');
  var metadataStore = transaction.objectStore('metadata');
  var dataStore = transaction.objectStore('data');
  var metadataRequest = metadataStore.get(key);
  var dataRequest = dataStore.get(key);

  var metadataEntry = null;
  var metadataReceived = false;
  var dataEntry = null;
  var dataReceived = false;

  var onPartialSuccess = function() {
    // Check if all sub-requests have finished.
    if (!metadataReceived || !dataReceived)
      return;

    // Check if both entries are available or both unavailable.
    if (!!metadataEntry != !!dataEntry) {
      console.warn('Incosistent cache database.');
      onFailure();
      return;
    }

    // Process the responses.
    if (!metadataEntry) {
      // The image not found.
      onFailure();
    } else if (metadataEntry.timestamp != timestamp) {
      // The image is not up to date, so remove it.
      this.removeImage(key, function() {}, function() {}, transaction);
      onFailure();
    } else {
      // The image is available. Update the last load time and return the
      // image data.
      metadataEntry.lastLoadTimestamp = Date.now();
      metadataStore.put(metadataEntry);  // Added asynchronously.
      onSuccess(dataEntry.data);
    }
  }.bind(this);

  metadataRequest.onsuccess = function(e) {
    if (e.target.result)
      metadataEntry = e.target.result;
    metadataReceived = true;
    onPartialSuccess();
  };

  dataRequest.onsuccess = function(e) {
    if (e.target.result)
      dataEntry = e.target.result;
    dataReceived = true;
    onPartialSuccess();
  };

  metadataRequest.onerror = function() {
    console.error('Failed to fetch metadata from the database.');
    metadataReceived = true;
    onPartialSuccess();
  };

  dataRequest.onerror = function() {
    console.error('Failed to fetch image data from the database.');
    dataReceived = true;
    onPartialSuccess();
  };
};

/**
 * Removes the image from the cache.
 * @param {string} key Cache key.
 * @param {function()=} opt_onSuccess Success callback.
 * @param {function()=} opt_onFailure Failure callback.
 * @param {IDBTransaction=} opt_transaction Transaction to be reused. If not
 *     provided, then a new one is created.
 */
ImageLoader.Cache.prototype.removeImage = function(
    key, opt_onSuccess, opt_onFailure, opt_transaction) {
  if (!this.db_) {
    console.warn('Cache database not available.');
    return;
  }

  var transaction = opt_transaction ||
      this.db_.transaction(['settings', 'metadata', 'data'], 'readwrite');
  var metadataStore = transaction.objectStore('metadata');
  var dataStore = transaction.objectStore('data');

  var cacheSize = null;
  var cacheSizeReceived = false;
  var metadataEntry = null;
  var metadataReceived = false;

  var onPartialSuccess = function() {
    if (!cacheSizeReceived || !metadataReceived)
      return;

    // If either cache size or metadata entry is not available, then it is
    // an error.
    if (cacheSize === null || !metadataEntry) {
      if (opt_onFailure)
        onFailure();
      return;
    }

    if (opt_onSuccess)
      opt_onSuccess();

    this.setCacheSize_(cacheSize - metadataEntry.size, transaction);
    metadataStore.delete(key);  // Delete asynchronously.
    dataStore.delete(key);  // Delete asynchronously.
  }.bind(this);

  var onCacheSizeFailure = function() {
    cacheSizeReceived = true;
  };

  var onCacheSizeSuccess = function(result) {
    cacheSize = result;
    cacheSizeReceived = true;
    onPartialSuccess();
  };

  // Fetch the current cache size.
  this.fetchCacheSize_(onCacheSizeSuccess, onCacheSizeFailure, transaction);

  // Receive image's metadata.
  var metadataRequest = metadataStore.get(key);

  metadataRequest.onsuccess = function(e) {
    if (e.target.result)
      metadataEntry = e.target.result;
    metadataReceived = true;
    onPartialSuccess();
  };

  metadataRequest.onerror = function() {
    console.error('Failed to remove an image.');
    metadataReceived = true;
    onPartialSuccess();
  };
};

// Load the extension.
ImageLoader.getInstance();
