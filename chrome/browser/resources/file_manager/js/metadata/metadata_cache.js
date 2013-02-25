// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * MetadataCache is a map from url to an object containing properties.
 * Properties are divided by types, and all properties of one type are accessed
 * at once.
 * Some of the properties:
 * {
 *   filesystem: size, modificationTime
 *   internal: presence
 *   drive: pinned, present, hosted, editUrl, contentUrl, availableOffline
 *   streaming: url
 *
 *   Following are not fetched for non-present drive files.
 *   media: artist, album, title, width, height, imageTransform, etc.
 *   thumbnail: url, transform
 *
 *   Following are always fetched from content, and so force the downloading
 *   of remote drive files. One should use this for required content metadata,
 *   i.e. image orientation.
 *   fetchedMedia: width, height, etc.
 * }
 *
 * Typical usages:
 * {
 *   cache.get([entry1, entry2], 'drive|filesystem', function(metadata) {
 *     if (metadata[0].drive.pinned && metadata[1].filesystem.size == 0)
 *       alert("Pinned and empty!");
 *   });
 *
 *   cache.set(entry, 'internal', {presence: 'deleted'});
 *
 *   cache.clear([fileUrl1, fileUrl2], 'filesystem');
 *
 *   // Getting fresh value.
 *   cache.clear(entry, 'thumbnail');
 *   cache.get(entry, 'thumbnail', function(thumbnail) {
 *     img.src = thumbnail.url;
 *   });
 *
 *   var cached = cache.getCached(entry, 'filesystem');
 *   var size = (cached && cached.size) || UNKNOWN_SIZE;
 * }
 *
 * @constructor
 */
function MetadataCache() {
  /**
   * Map from urls to entries. Entry contains |properties| - an hierarchical
   * object of values, and an object for each metadata provider:
   * <prodiver-id>: { time, callbacks }
   * @private
   */
  this.cache_ = {};

  /**
   * List of metadata providers.
   * @private
   */
  this.providers_ = [];

  /**
   * List of observers added. Each one is an object with fields:
   *   re - regexp of urls;
   *   type - metadata type;
   *   callback - the callback.
   * TODO(dgozman): pass entries to observer if present.
   * @private
   */
  this.observers_ = [];
  this.observerId_ = 0;

  this.batchCount_ = 0;
  this.totalCount_ = 0;

  /**
   * Time of first get query of the current batch. Items updated later than this
   * will not be evicted.
   * @private
   */
  this.lastBatchStart_ = new Date();

  // Holds the directories known to contain files with stale metadata
  // as URL to bool map.
  this.directoriesWithStaleMetadata_ = {};
}

/**
 * Observer type: it will be notified if the url changed is exactly the same
 * as the url passed.
 */
MetadataCache.EXACT = 0;

/**
 * Observer type: it will be notified if the url changed is an immediate child
 * of the url passed.
 */
MetadataCache.CHILDREN = 1;

/**
 * Observer type: it will be notified if the url changed is any descendant
 * of the url passed.
 */
MetadataCache.DESCENDANTS = 2;

/**
 * Minimum number of items in cache to start eviction.
 */
MetadataCache.EVICTION_NUMBER = 1000;

/**
 * @return {MetadataCache!} The cache with all providers.
 */
MetadataCache.createFull = function() {
  var cache = new MetadataCache();
  cache.providers_.push(new FilesystemProvider());
  cache.providers_.push(new DriveProvider());
  cache.providers_.push(new ContentProvider());
  return cache;
};

/**
 * @return {boolean} Whether all providers are ready.
 */
MetadataCache.prototype.isInitialized = function() {
  for (var index = 0; index < this.providers_.length; index++) {
    if (!this.providers_[index].isInitialized()) return false;
  }
  return true;
};

/**
 * Fetches the metadata, puts it in the cache, and passes to callback.
 * If required metadata is already in the cache, does not fetch it again.
 * @param {string|Entry|Array.<string|Entry>} items The list of entries or
 *     file urls. May be just a single item.
 * @param {string} type The metadata type.
 * @param {function(Object)} callback The metadata is passed to callback.
 */
MetadataCache.prototype.get = function(items, type, callback) {
  if (!(items instanceof Array)) {
    this.getOne(items, type, callback);
    return;
  }

  if (items.length == 0) {
    if (callback) callback([]);
    return;
  }

  var result = [];
  var remaining = items.length;
  this.startBatchUpdates();

  var onOneItem = function(index, value) {
    result[index] = value;
    remaining--;
    if (remaining == 0) {
      this.endBatchUpdates();
      if (callback) setTimeout(callback, 0, result);
    }
  };

  for (var index = 0; index < items.length; index++) {
    result.push(null);
    this.getOne(items[index], type, onOneItem.bind(this, index));
  }
};

/**
 * Fetches the metadata for one Entry/FileUrl. See comments to |get|.
 * @param {Entry|string} item The entry or url.
 * @param {string} type Metadata type.
 * @param {function(Object)} callback The callback.
 */
MetadataCache.prototype.getOne = function(item, type, callback) {
  if (type.indexOf('|') != -1) {
    var types = type.split('|');
    var result = {};
    var typesLeft = types.length;

    var onOneType = function(requestedType, metadata) {
      result[requestedType] = metadata;
      typesLeft--;
      if (typesLeft == 0) callback(result);
    };

    for (var index = 0; index < types.length; index++) {
      this.getOne(item, types[index], onOneType.bind(null, types[index]));
    }
    return;
  }

  var url = this.itemToUrl_(item);

  // Passing entry to fetchers may save one round-trip to APIs.
  var fsEntry = item === url ? null : item;
  callback = callback || function() {};

  if (!(url in this.cache_)) {
    this.cache_[url] = this.createEmptyEntry_();
    this.totalCount_++;
  }

  var entry = this.cache_[url];

  if (type in entry.properties) {
    callback(entry.properties[type]);
    return;
  }

  this.startBatchUpdates();
  var providers = this.providers_.slice();
  var currentProvider;
  var self = this;

  var onFetched = function() {
    if (type in entry.properties) {
      self.endBatchUpdates();
      // Got properties from provider.
      callback(entry.properties[type]);
    } else {
      tryNextProvider();
    }
  };

  var onProviderProperties = function(properties) {
    var id = currentProvider.getId();
    var fetchedCallbacks = entry[id].callbacks;
    delete entry[id].callbacks;
    entry.time = new Date();
    self.mergeProperties_(url, properties);

    for (var index = 0; index < fetchedCallbacks.length; index++) {
      fetchedCallbacks[index]();
    }
  };

  var queryProvider = function() {
    var id = currentProvider.getId();
    if ('callbacks' in entry[id]) {
      // We are querying this provider now.
      entry[id].callbacks.push(onFetched);
    } else {
      entry[id].callbacks = [onFetched];
      currentProvider.fetch(url, type, onProviderProperties, fsEntry);
    }
  };

  var tryNextProvider = function() {
    if (providers.length == 0) {
      self.endBatchUpdates();
      callback(entry.properties[type] || null);
      return;
    }

    currentProvider = providers.shift();
    if (currentProvider.supportsUrl(url) &&
        currentProvider.providesType(type)) {
      queryProvider();
    } else {
      tryNextProvider();
    }
  };

  tryNextProvider();
};

/**
 * Returns the cached metadata value, or |null| if not present.
 * @param {string|Entry|Array.<string|Entry>} items The list of entries or
 *     file urls. May be just a single item.
 * @param {string} type The metadata type.
 * @return {Object} The metadata or null.
 */
MetadataCache.prototype.getCached = function(items, type) {
  var single = false;
  if (!(items instanceof Array)) {
    single = true;
    items = [items];
  }

  var result = [];
  for (var index = 0; index < items.length; index++) {
    var url = this.itemToUrl_(items[index]);
    result.push(url in this.cache_ ?
        (this.cache_[url].properties[type] || null) : null);
  }

  return single ? result[0] : result;
};

/**
 * Puts the metadata into cache
 * @param {string|Entry|Array.<string|Entry>} items The list of entries or
 *     file urls. May be just a single item.
 * @param {string} type The metadata type.
 * @param {Array.<Object>} values List of corresponding metadata values.
 */
MetadataCache.prototype.set = function(items, type, values) {
  if (!(items instanceof Array)) {
    items = [items];
    values = [values];
  }

  this.startBatchUpdates();
  for (var index = 0; index < items.length; index++) {
    var url = this.itemToUrl_(items[index]);
    if (!(url in this.cache_)) {
      this.cache_[url] = this.createEmptyEntry_();
      this.totalCount_++;
    }
    this.cache_[url].properties[type] = values[index];
    this.notifyObservers_(url, type);
  }
  this.endBatchUpdates();
};

/**
 * Clears the cached metadata values.
 * @param {string|Entry|Array.<string|Entry>} items The list of entries or
 *     file urls. May be just a single item.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clear = function(items, type) {
  if (!(items instanceof Array))
    items = [items];

  var types = type.split('|');

  for (var index = 0; index < items.length; index++) {
    var url = this.itemToUrl_(items[index]);
    if (url in this.cache_) {
      if (type === '*') {
        this.cache_[url].properties = {};
      } else {
        for (var j = 0; j < types.length; j++) {
          var type = types[j];
          delete this.cache_[url].properties[type];
        }
      }
    }
  }
};

/**
 * Clears the cached metadata values recursively.
 * @param {Entry|string} item An entry or a url.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clearRecursively = function(item, type) {
  var types = type.split('|');
  var keys = Object.keys(this.cache_);
  var url = this.itemToUrl_(item);

  for (entryUrl in keys) {
    if (entryUrl.substring(0, url.length) === url) {
      if (type === '*') {
        this.cache_[entryUrl].properties = {};
      } else {
        for (var index = 0; index < types.length; index++) {
          var type = types[index];
          delete this.cache_[entryUrl].properties[type];
        }
      }
    }
  }
};

/**
 * Adds an observer, which will be notified when metadata changes.
 * @param {string|Entry} item The root item to look at.
 * @param {number} relation This defines, which items will trigger the observer.
 *     See comments to |MetadataCache.EXACT| and others.
 * @param {string} type The metadata type.
 * @param {function(Array.<string>, Array.<Object>)} observer List of file urls
 *     and corresponding metadata values are passed to this callback.
 * @return {number} The observer id, which can be used to remove it.
 */
MetadataCache.prototype.addObserver = function(item, relation, type, observer) {
  var url = this.itemToUrl_(item);
  var re = url;
  if (relation == MetadataCache.CHILDREN) {
    re += '(/[^/]*)?';
  } else if (relation == MetadataCache.DESCENDANTS) {
    re += '(/.*)?';
  }
  var id = ++this.observerId_;
  this.observers_.push({
    re: new RegExp('^' + re + '$'),
    type: type,
    callback: observer,
    id: id,
    pending: {}
  });
  return id;
};

/**
 * Removes the observer.
 * @param {number} id Observer id.
 * @return {boolean} Whether observer was removed or not.
 */
MetadataCache.prototype.removeObserver = function(id) {
  for (var index = 0; index < this.observers_.length; index++) {
    if (this.observers_[index].id == id) {
      this.observers_.splice(index, 1);
      return true;
    }
  }
  return false;
};

/**
 * Start batch updates.
 */
MetadataCache.prototype.startBatchUpdates = function() {
  this.batchCount_++;
  if (this.batchCount_ == 1)
    this.lastBatchStart_ = new Date();
};

/**
 * End batch updates. Notifies observers if all nested updates are finished.
 */
MetadataCache.prototype.endBatchUpdates = function() {
  this.batchCount_--;
  if (this.batchCount_ != 0) return;
  if (this.totalCount_ > MetadataCache.EVICTION_NUMBER)
    this.evict_();
  for (var index = 0; index < this.observers_.length; index++) {
    var observer = this.observers_[index];
    var urls = [];
    var properties = [];
    for (var url in observer.pending) {
      if (observer.pending.hasOwnProperty(url) && url in this.cache_) {
        urls.push(url);
        properties.push(this.cache_[url].properties[observer.type] || null);
      }
    }
    observer.pending = {};
    if (urls.length > 0) {
      observer.callback(urls, properties);
    }
  }
};

/**
 * Notifies observers or puts the data to pending list.
 * @param {string} url Url of entry changed.
 * @param {string} type Metadata type.
 * @private
 */
MetadataCache.prototype.notifyObservers_ = function(url, type) {
  for (var index = 0; index < this.observers_.length; index++) {
    var observer = this.observers_[index];
    if (observer.type == type && observer.re.test(url)) {
      if (this.batchCount_ == 0) {
        // Observer expects array of urls and array of properties.
        observer.callback([url], [this.cache_[url].properties[type] || null]);
      } else {
        observer.pending[url] = true;
      }
    }
  }
};

/**
 * Removes the oldest items from the cache.
 * This method never removes the items from last batch.
 * @private
 */
MetadataCache.prototype.evict_ = function() {
  var toRemove = [];

  // We leave only a half of items, so we will not call evict_ soon again.
  var desiredCount = Math.round(MetadataCache.EVICTION_NUMBER / 2);
  var removeCount = this.totalCount_ - desiredCount;
  for (var url in this.cache_) {
    if (this.cache_.hasOwnProperty(url) &&
        this.cache_[url].time < this.lastBatchStart_) {
      toRemove.push(url);
    }
  }

  toRemove.sort(function(a, b) {
    var aTime = this.cache_[a].time;
    var bTime = this.cache_[b].time;
    return aTime < bTime ? -1 : aTime > bTime ? 1 : 0;
  }.bind(this));

  removeCount = Math.min(removeCount, toRemove.length);
  this.totalCount_ -= removeCount;
  for (var index = 0; index < removeCount; index++) {
    delete this.cache_[toRemove[index]];
  }
};

/**
 * Converts Entry or file url to url.
 * @param {string|Entry} item Item to convert.
 * @return {string} File url.
 * @private
 */
MetadataCache.prototype.itemToUrl_ = function(item) {
  if (typeof(item) == 'string')
    return item;

  if (!item._URL_) {
    // Is a fake entry.
    if (typeof item.toURL !== 'function')
      item._URL_ = util.makeFilesystemUrl(item.fullPath);
    else
      item._URL_ = item.toURL();
  }

  return item._URL_;
};

/**
 * @return {Object} Empty cache entry.
 * @private
 */
MetadataCache.prototype.createEmptyEntry_ = function() {
  var entry = {properties: {}};
  for (var index = 0; index < this.providers_.length; index++) {
    entry[this.providers_[index].getId()] = {};
  }
  return entry;
};

/**
 * Caches all the properties from data to cache entry for url.
 * @param {string} url The url.
 * @param {Object} data The properties.
 * @private
 */
MetadataCache.prototype.mergeProperties_ = function(url, data) {
  if (data == null) return;
  var properties = this.cache_[url].properties;
  for (var type in data) {
    if (data.hasOwnProperty(type) && !properties.hasOwnProperty(type)) {
      properties[type] = data[type];
      this.notifyObservers_(url, type);
    }
  }
};

/**
 * Ask the Drive service to re-fetch the metadata. Ignores sequential requests.
 * @param {string} url Directory URL.
 */
MetadataCache.prototype.refreshDirectory = function(url) {
  // Skip if the current directory is now being refreshed.
  if (this.directoriesWithStaleMetadata_[url] || !FileType.isOnDrive(url))
    return;

  this.directoriesWithStaleMetadata_[url] = true;
  chrome.fileBrowserPrivate.requestDirectoryRefresh(url);
};

/**
 * Ask the Drive service to re-fetch the metadata.
 * @param {string} fileURL File URL.
 */
MetadataCache.prototype.refreshFileMetadata = function(fileURL) {
  if (!FileType.isOnDrive(fileURL))
    return;
  // TODO(kaznacheev) This does not really work with Drive search.
  var url = fileURL.substr(0, fileURL.lastIndexOf('/'));
  this.refreshDirectory(url);
};

/**
 * Resumes refreshes by resreshDirectory.
 * @param {string} url Directory URL.
 */
MetadataCache.prototype.resumeRefresh = function(url) {
  delete this.directoriesWithStaleMetadata_[url];
};

/**
 * Base class for metadata providers.
 * @constructor
 */
function MetadataProvider() {
}

/**
 * @param {string} url The url.
 * @return {boolean} Whether this provider supports the url.
 */
MetadataProvider.prototype.supportsUrl = function(url) { return false; };

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
MetadataProvider.prototype.providesType = function(type) { return false; };

/**
 * @return {string} Unique provider id.
 */
MetadataProvider.prototype.getId = function() { return ''; };

/**
 * @return {boolean} Whether provider is ready.
 */
MetadataProvider.prototype.isInitialized = function() { return true; };

/**
 * Fetches the metadata. It's suggested to return all the metadata this provider
 * can fetch at once.
 * @param {string} url File url.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 * @param {Entry=} opt_entry The file entry if present.
 */
MetadataProvider.prototype.fetch = function(url, type, callback, opt_entry) {
  throw new Error('Default metadata provider cannot fetch.');
};


/**
 * Provider of filesystem metadata.
 * This provider returns the following objects:
 * filesystem: { size, modificationTime }
 * @constructor
 */
function FilesystemProvider() {
  MetadataProvider.call(this);
}

FilesystemProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {string} url The url.
 * @return {boolean} Whether this provider supports the url.
 */
FilesystemProvider.prototype.supportsUrl = function(url) {
  return true;
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
FilesystemProvider.prototype.providesType = function(type) {
  return type == 'filesystem';
};

/**
 * @return {string} Unique provider id.
 */
FilesystemProvider.prototype.getId = function() { return 'filesystem'; };

/**
 * Fetches the metadata.
 * @param {string} url File url.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 * @param {Entry=} opt_entry The file entry if present.
 */
FilesystemProvider.prototype.fetch = function(url, type, callback, opt_entry) {
  function onError(error) {
    callback(null);
  }

  function onMetadata(entry, metadata) {
    callback({
      filesystem: {
        size: entry.isFile ? (metadata.size || 0) : -1,
        modificationTime: metadata.modificationTime
      }
    });
  }

  function onEntry(entry) {
    entry.getMetadata(onMetadata.bind(null, entry), onError);
  }

  if (opt_entry)
    onEntry(opt_entry);
  else
    window.webkitResolveLocalFileSystemURL(url, onEntry, onError);
};

/**
 * Provider of drive metadata.
 * This provider returns the following objects:
 *     drive: { pinned, hosted, present, dirty, editUrl, contentUrl, driveApps }
 *     thumbnail: { url, transform }
 *     streaming: { url }
 * @constructor
 */
function DriveProvider() {
  MetadataProvider.call(this);

  // We batch metadata fetches into single API call.
  this.urls_ = [];
  this.callbacks_ = [];
  this.scheduled_ = false;

  this.callApiBound_ = this.callApi_.bind(this);
}

DriveProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {string} url The url.
 * @return {boolean} Whether this provider supports the url.
 */
DriveProvider.prototype.supportsUrl = function(url) {
  return FileType.isOnDrive(url);
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
DriveProvider.prototype.providesType = function(type) {
  return type == 'drive' || type == 'thumbnail' ||
      type == 'streaming' || type == 'media';
};

/**
 * @return {string} Unique provider id.
 */
DriveProvider.prototype.getId = function() { return 'drive'; };

/**
 * Fetches the metadata.
 * @param {string} url File url.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 * @param {Entry=} opt_entry The file entry if present.
 */
DriveProvider.prototype.fetch = function(url, type, callback, opt_entry) {
  this.urls_.push(url);
  this.callbacks_.push(callback);
  if (!this.scheduled_) {
    this.scheduled_ = true;
    setTimeout(this.callApiBound_, 0);
  }
};

/**
 * Schedules the API call.
 * @private
 */
DriveProvider.prototype.callApi_ = function() {
  this.scheduled_ = false;

  var urls = this.urls_;
  var callbacks = this.callbacks_;
  this.urls_ = [];
  this.callbacks_ = [];
  var self = this;

  chrome.fileBrowserPrivate.getDriveFileProperties(urls, function(props) {
    for (var index = 0; index < urls.length; index++) {
      callbacks[index](self.convert_(props[index], urls[index]));
    }
  });
};

/**
 * @param {DriveFileProperties} data Drive file properties.
 * @param {string} url File url.
 * @return {boolean} True if the file is available offline.
 */
DriveProvider.isAvailableOffline = function(data, url) {
  if (data.isPresent)
    return true;

  if (!data.isHosted)
    return false;

  var subtype = FileType.getType(url).subtype;
  return subtype == 'doc' || subtype == 'sheet';
};

/**
 * @param {DriveFileProperties} data Drive file properties.
 * @return {boolean} True if opening the file does not require downloading it
 *    via a metered connection.
 */
DriveProvider.isAvailableWhenMetered = function(data) {
  return data.isPresent || data.isHosted;
};

/**
 * Converts API metadata to internal format.
 * @param {Object} data Metadata from API call.
 * @param {string} url File url.
 * @return {Object} Metadata in internal format.
 * @private
 */
DriveProvider.prototype.convert_ = function(data, url) {
  var result = {};
  result.drive = {
    present: data.isPresent,
    pinned: data.isPinned,
    hosted: data.isHosted,
    dirty: data.isDirty,
    availableOffline: DriveProvider.isAvailableOffline(data, url),
    availableWhenMetered: DriveProvider.isAvailableWhenMetered(data),
    contentUrl: (data.contentUrl || '').replace(/\?.*$/gi, ''),
    editUrl: data.editUrl || '',
    driveApps: data.driveApps || [],
    contentMimeType: data.contentMimeType || ''
  };

  if (!data.isPresent) {
    // Block the local fetch for drive files, which require downloading.
    result.thumbnail = { url: '', transform: null };
    result.media = {};
  }

  if ('thumbnailUrl' in data) {
    result.thumbnail = {
      url: data.thumbnailUrl.replace(/s220/, 's500'),
      transform: null
    };
  }
  if (!data.isPresent && ('contentUrl' in data)) {
    result.streaming = {
      url: data.contentUrl.replace(/\?.*$/gi, '')
    };
  }
  return result;
};


/**
 * Provider of content metadata.
 * This provider returns the following objects:
 * thumbnail: { url, transform }
 * media: { artist, album, title, width, height, imageTransform, etc. }
 * fetchedMedia: { same fields here }
 * @constructor
 */
function ContentProvider() {
  MetadataProvider.call(this);

  // Pass all URLs to the metadata reader until we have a correct filter.
  this.urlFilter_ = /.*/;

  var path = document.location.pathname;
  var workerPath = document.location.origin +
      path.substring(0, path.lastIndexOf('/') + 1) +
      'js/metadata/metadata_dispatcher.js';

  if (ContentProvider.USE_SHARED_WORKER) {
    this.dispatcher_ = new SharedWorker(workerPath).port;
    this.dispatcher_.start();
  } else {
    this.dispatcher_ = new Worker(workerPath);
  }

  this.dispatcher_.onmessage = this.onMessage_.bind(this);
  this.dispatcher_.postMessage({verb: 'init'});

  // Initialization is not complete until the Worker sends back the
  // 'initialized' message.  See below.
  this.initialized_ = false;

  // Map from url to callback.
  // Note that simultaneous requests for same url are handled in MetadataCache.
  this.callbacks_ = {};
}

/**
 * Flag defining which kind of a worker to use.
 * TODO(kaznacheev): Observe for some time and remove if SharedWorker does not
 * cause any problems.
 */
ContentProvider.USE_SHARED_WORKER = true;

ContentProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {string} url The url.
 * @return {boolean} Whether this provider supports the url.
 */
ContentProvider.prototype.supportsUrl = function(url) {
  return url.match(this.urlFilter_);
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
ContentProvider.prototype.providesType = function(type) {
  return type == 'thumbnail' || type == 'fetchedMedia' || type == 'media';
};

/**
 * @return {string} Unique provider id.
 */
ContentProvider.prototype.getId = function() { return 'content'; };

/**
 * Fetches the metadata.
 * @param {string} url File url.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 * @param {Entry=} opt_entry The file entry if present.
 */
ContentProvider.prototype.fetch = function(url, type, callback, opt_entry) {
  if (opt_entry && opt_entry.isDirectory) {
    callback({});
    return;
  }
  this.callbacks_[url] = callback;
  this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
};

/**
 * Dispatch a message from a metadata reader to the appropriate on* method.
 * @param {Object} event The event.
 * @private
 */
ContentProvider.prototype.onMessage_ = function(event) {
  var data = event.data;

  var methodName =
      'on' + data.verb.substr(0, 1).toUpperCase() + data.verb.substr(1) + '_';

  if (!(methodName in this)) {
    console.error('Unknown message from metadata reader: ' + data.verb, data);
    return;
  }

  this[methodName].apply(this, data.arguments);
};

/**
 * @return {boolean} Whether provider is ready.
 */
ContentProvider.prototype.isInitialized = function() {
  return this.initialized_;
};

/**
 * Handles the 'initialized' message from the metadata reader Worker.
 * @param {Object} regexp Regexp of supported urls.
 * @private
 */
ContentProvider.prototype.onInitialized_ = function(regexp) {
  this.urlFilter_ = regexp;

  // Tests can monitor for this state with
  // ExtensionTestMessageListener listener("worker-initialized");
  // ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Automated tests need to wait for this, otherwise we crash in
  // browser_test cleanup because the worker process still has
  // URL requests in-flight.
  var test = chrome.test || window.top.chrome.test;
  test.sendMessage('worker-initialized');
  this.initialized_ = true;
};

/**
 * Converts content metadata from parsers to the internal format.
 * @param {Object} metadata The content metadata.
 * @param {Object=} opt_result The internal metadata object ot put result in.
 * @return {Object!} Converted metadata.
 */
ContentProvider.ConvertContentMetadata = function(metadata, opt_result) {
  var result = opt_result || {};

  if ('thumbnailURL' in metadata) {
    metadata.thumbnailTransform = metadata.thumbnailTransform || null;
    result.thumbnail = {
      url: metadata.thumbnailURL,
      transform: metadata.thumbnailTransform
    };
  }

  for (var key in metadata) {
    if (metadata.hasOwnProperty(key)) {
      if (!('media' in result)) result.media = {};
      result.media[key] = metadata[key];
    }
  }

  if ('media' in result) {
    result.fetchedMedia = result.media;
  }

  return result;
};

/**
 * Handles the 'result' message from the worker.
 * @param {string} url File url.
 * @param {Object} metadata The metadata.
 * @private
 */
ContentProvider.prototype.onResult_ = function(url, metadata) {
  var callback = this.callbacks_[url];
  delete this.callbacks_[url];
  callback(ContentProvider.ConvertContentMetadata(metadata));
};

/**
 * Handles the 'error' message from the worker.
 * @param {string} url File url.
 * @param {string} step Step failed.
 * @param {string} error Error description.
 * @param {Object?} metadata The metadata, if available.
 * @private
 */
ContentProvider.prototype.onError_ = function(url, step, error, metadata) {
  if (MetadataCache.log)  // Avoid log spam by default.
    console.warn('metadata: ' + url + ': ' + step + ': ' + error);
  metadata = metadata || {};
  // Prevent asking for thumbnail again.
  metadata.thumbnailURL = '';
  this.onResult_(url, metadata);
};

/**
 * Handles the 'log' message from the worker.
 * @param {Array.<*>} arglist Log arguments.
 * @private
 */
ContentProvider.prototype.onLog_ = function(arglist) {
  if (MetadataCache.log)  // Avoid log spam by default.
    console.log.apply(console, ['metadata:'].concat(arglist));
};
