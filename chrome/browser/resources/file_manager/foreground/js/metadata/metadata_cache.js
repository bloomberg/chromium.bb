// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * MetadataCache is a map from Entry to an object containing properties.
 * Properties are divided by types, and all properties of one type are accessed
 * at once.
 * Some of the properties:
 * {
 *   filesystem: size, modificationTime
 *   internal: presence
 *   drive: pinned, present, hosted, availableOffline
 *   streaming: (no property)
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
 *     if (metadata[0].drive.pinned && metadata[1].filesystem.size === 0)
 *       alert("Pinned and empty!");
 *   });
 *
 *   cache.set(entry, 'internal', {presence: 'deleted'});
 *
 *   cache.clear([fileEntry1, fileEntry2], 'filesystem');
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
   * Map from Entry (using Entry.toURL) to metadata. Metadata contains
   * |properties| - an hierarchical object of values, and an object for each
   * metadata provider: <prodiver-id>: {time, callbacks}
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
   * @private
   */
  this.observers_ = [];
  this.observerId_ = 0;

  this.batchCount_ = 0;
  this.totalCount_ = 0;

  this.currentCacheSize_ = 0;

  /**
   * Time of first get query of the current batch. Items updated later than this
   * will not be evicted.
   * @private
   */
  this.lastBatchStart_ = new Date();
}

/**
 * Observer type: it will be notified if the changed Entry is exactly the same
 * as the observed Entry.
 */
MetadataCache.EXACT = 0;

/**
 * Observer type: it will be notified if the changed Entry is an immediate child
 * of the observed Entry.
 */
MetadataCache.CHILDREN = 1;

/**
 * Observer type: it will be notified if the changed Entry is a descendant of
 * of the observer Entry.
 */
MetadataCache.DESCENDANTS = 2;

/**
 * Margin of the cache size. This amount of caches may be kept in addition.
 */
MetadataCache.EVICTION_THRESHOLD_MARGIN = 500;

/**
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @return {MetadataCache!} The cache with all providers.
 */
MetadataCache.createFull = function(volumeManager) {
  var cache = new MetadataCache();
  cache.providers_.push(new FilesystemProvider());
  cache.providers_.push(new DriveProvider(volumeManager));
  cache.providers_.push(new ContentProvider());
  return cache;
};

/**
 * Clones metadata entry. Metadata entries may contain scalars, arrays,
 * hash arrays and Date object. Other objects are not supported.
 * @param {Object} metadata Metadata object.
 * @return {Object} Cloned entry.
 */
MetadataCache.cloneMetadata = function(metadata) {
  if (metadata instanceof Array) {
    var result = [];
    for (var index = 0; index < metadata.length; index++) {
      result[index] = MetadataCache.cloneMetadata(metadata[index]);
    }
    return result;
  } else if (metadata instanceof Date) {
    var result = new Date();
    result.setTime(metadata.getTime());
    return result;
  } else if (metadata instanceof Object) {  // Hash array only.
    var result = {};
    for (var property in metadata) {
      if (metadata.hasOwnProperty(property))
        result[property] = MetadataCache.cloneMetadata(metadata[property]);
    }
    return result;
  } else {
    return metadata;
  }
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
 * Sets the size of cache. The actual cache size may be larger than the given
 * value.
 * @param {number} size The cache size to be set.
 */
MetadataCache.prototype.setCacheSize = function(size) {
  this.currentCacheSize_ = size;

  if (this.totalCount_ > this.currentEvictionThreshold_())
    this.evict_();
};

/**
 * Returns the current threshold to evict caches. When the number of caches
 * exceeds this, the cache should be evicted.
 * @return {number} Threshold to evict caches.
 * @private
 */
MetadataCache.prototype.currentEvictionThreshold_ = function() {
  return this.currentCacheSize_ * 2 + MetadataCache.EVICTION_THRESHOLD_MARGIN;
};

/**
 * Fetches the metadata, puts it in the cache, and passes to callback.
 * If required metadata is already in the cache, does not fetch it again.
 * @param {Entry|Array.<Entry>} entries The list of entries. May be just a
 *     single item.
 * @param {string} type The metadata type.
 * @param {function(Object)} callback The metadata is passed to callback.
 */
MetadataCache.prototype.get = function(entries, type, callback) {
  if (!(entries instanceof Array)) {
    this.getOne(entries, type, callback);
    return;
  }

  if (entries.length === 0) {
    if (callback) callback([]);
    return;
  }

  var result = [];
  var remaining = entries.length;
  this.startBatchUpdates();

  var onOneItem = function(index, value) {
    result[index] = value;
    remaining--;
    if (remaining === 0) {
      this.endBatchUpdates();
      if (callback) setTimeout(callback, 0, result);
    }
  };

  for (var index = 0; index < entries.length; index++) {
    result.push(null);
    this.getOne(entries[index], type, onOneItem.bind(this, index));
  }
};

/**
 * Fetches the metadata for one Entry. See comments to |get|.
 * @param {Entry} entry The entry.
 * @param {string} type Metadata type.
 * @param {function(Object)} callback The callback.
 */
MetadataCache.prototype.getOne = function(entry, type, callback) {
  if (type.indexOf('|') !== -1) {
    var types = type.split('|');
    var result = {};
    var typesLeft = types.length;

    var onOneType = function(requestedType, metadata) {
      result[requestedType] = metadata;
      typesLeft--;
      if (typesLeft === 0) callback(result);
    };

    for (var index = 0; index < types.length; index++) {
      this.getOne(entry, types[index], onOneType.bind(null, types[index]));
    }
    return;
  }

  callback = callback || function() {};

  var entryURL = entry.toURL();
  if (!(entryURL in this.cache_)) {
    this.cache_[entryURL] = this.createEmptyItem_();
    this.totalCount_++;
  }

  var item = this.cache_[entryURL];

  if (type in item.properties) {
    callback(item.properties[type]);
    return;
  }

  this.startBatchUpdates();
  var providers = this.providers_.slice();
  var currentProvider;
  var self = this;

  var onFetched = function() {
    if (type in item.properties) {
      self.endBatchUpdates();
      // Got properties from provider.
      callback(item.properties[type]);
    } else {
      tryNextProvider();
    }
  };

  var onProviderProperties = function(properties) {
    var id = currentProvider.getId();
    var fetchedCallbacks = item[id].callbacks;
    delete item[id].callbacks;
    item.time = new Date();
    self.mergeProperties_(entry, properties);

    for (var index = 0; index < fetchedCallbacks.length; index++) {
      fetchedCallbacks[index]();
    }
  };

  var queryProvider = function() {
    var id = currentProvider.getId();
    if ('callbacks' in item[id]) {
      // We are querying this provider now.
      item[id].callbacks.push(onFetched);
    } else {
      item[id].callbacks = [onFetched];
      currentProvider.fetch(entry, type, onProviderProperties);
    }
  };

  var tryNextProvider = function() {
    if (providers.length === 0) {
      self.endBatchUpdates();
      callback(item.properties[type] || null);
      return;
    }

    currentProvider = providers.shift();
    if (currentProvider.supportsEntry(entry) &&
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
 * @param {Entry|Array.<Entry>} entries The list of entries. May be just a
 *     single entry.
 * @param {string} type The metadata type.
 * @return {Object} The metadata or null.
 */
MetadataCache.prototype.getCached = function(entries, type) {
  var single = false;
  if (!(entries instanceof Array)) {
    single = true;
    entries = [entries];
  }

  var result = [];
  for (var index = 0; index < entries.length; index++) {
    var entryURL = entries[index].toURL();
    result.push(entryURL in this.cache_ ?
        (this.cache_[entryURL].properties[type] || null) : null);
  }

  return single ? result[0] : result;
};

/**
 * Puts the metadata into cache
 * @param {Entry|Array.<Entry>} entries The list of entries. May be just a
 *     single entry.
 * @param {string} type The metadata type.
 * @param {Array.<Object>} values List of corresponding metadata values.
 */
MetadataCache.prototype.set = function(entries, type, values) {
  if (!(entries instanceof Array)) {
    entries = [entries];
    values = [values];
  }

  this.startBatchUpdates();
  for (var index = 0; index < entries.length; index++) {
    var entryURL = entries[index].toURL();
    if (!(entryURL in this.cache_)) {
      this.cache_[entryURL] = this.createEmptyItem_();
      this.totalCount_++;
    }
    this.cache_[entryURL].properties[type] = values[index];
    this.notifyObservers_(entries[index], type);
  }
  this.endBatchUpdates();
};

/**
 * Clears the cached metadata values.
 * @param {Entry|Array.<Entry>} entries The list of entries. May be just a
 *     single entry.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clear = function(entries, type) {
  if (!(entries instanceof Array))
    entries = [entries];

  var types = type.split('|');

  for (var index = 0; index < entries.length; index++) {
    var entry = entries[index];
    var entryURL = entry.toURL();
    if (entryURL in this.cache_) {
      if (type === '*') {
        this.cache_[entryURL].properties = {};
      } else {
        for (var j = 0; j < types.length; j++) {
          var type = types[j];
          delete this.cache_[entryURL].properties[type];
        }
      }
    }
  }
};

/**
 * Clears the cached metadata values recursively.
 * @param {Entry} entry An entry to be cleared recursively from cache.
 * @param {string} type The metadata types or * for any type.
 */
MetadataCache.prototype.clearRecursively = function(entry, type) {
  var types = type.split('|');
  var keys = Object.keys(this.cache_);
  var entryURL = entry.toURL();

  for (var index = 0; index < keys.length; index++) {
    var cachedEntryURL = keys[index];
    if (cachedEntryURL.substring(0, entryURL.length) === entryURL) {
      if (type === '*') {
        this.cache_[cachedEntryURL].properties = {};
      } else {
        for (var j = 0; j < types.length; j++) {
          var type = types[j];
          delete this.cache_[cachedEntryURL].properties[type];
        }
      }
    }
  }
};

/**
 * Adds an observer, which will be notified when metadata changes.
 * @param {Entry} entry The root entry to look at.
 * @param {number} relation This defines, which items will trigger the observer.
 *     See comments to |MetadataCache.EXACT| and others.
 * @param {string} type The metadata type.
 * @param {function(Array.<Entry>, Object.<string, Object>)} observer Map of
 *     entries and corresponding metadata values are passed to this callback.
 * @return {number} The observer id, which can be used to remove it.
 */
MetadataCache.prototype.addObserver = function(
    entry, relation, type, observer) {
  var entryURL = entry.toURL();
  var re;
  if (relation === MetadataCache.CHILDREN)
    re = entryURL + '(/[^/]*)?';
  else if (relation === MetadataCache.DESCENDANTS)
    re = entryURL + '(/.*)?';
  else
    re = entryURL;

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
    if (this.observers_[index].id === id) {
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
  if (this.batchCount_ === 1)
    this.lastBatchStart_ = new Date();
};

/**
 * End batch updates. Notifies observers if all nested updates are finished.
 */
MetadataCache.prototype.endBatchUpdates = function() {
  this.batchCount_--;
  if (this.batchCount_ !== 0) return;
  if (this.totalCount_ > this.currentEvictionThreshold_())
    this.evict_();
  for (var index = 0; index < this.observers_.length; index++) {
    var observer = this.observers_[index];
    var entries = [];
    var properties = {};
    for (var entryURL in observer.pending) {
      if (observer.pending.hasOwnProperty(entryURL) &&
          entryURL in this.cache_) {
        var entry = observer.pending[entryURL];
        entries.push(entry);
        properties[entryURL] =
            this.cache_[entryURL].properties[observer.type] || null;
      }
    }
    observer.pending = {};
    if (entries.length > 0) {
      observer.callback(entries, properties);
    }
  }
};

/**
 * Notifies observers or puts the data to pending list.
 * @param {Entry} entry Changed entry.
 * @param {string} type Metadata type.
 * @private
 */
MetadataCache.prototype.notifyObservers_ = function(entry, type) {
  var entryURL = entry.toURL();
  for (var index = 0; index < this.observers_.length; index++) {
    var observer = this.observers_[index];
    if (observer.type === type && observer.re.test(entryURL)) {
      if (this.batchCount_ === 0) {
        // Observer expects array of urls and map of properties.
        var property = {};
        property[entryURL] = this.cache_[entryURL].properties[type] || null;
        observer.callback(
            [entry], property);
      } else {
        observer.pending[entryURL] = entry;
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
  var desiredCount = this.currentEvictionThreshold_();
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
 * @return {Object} Empty cache item.
 * @private
 */
MetadataCache.prototype.createEmptyItem_ = function() {
  var item = {properties: {}};
  for (var index = 0; index < this.providers_.length; index++) {
    item[this.providers_[index].getId()] = {};
  }
  return item;
};

/**
 * Caches all the properties from data to cache entry for the entry.
 * @param {Entry} entry The file entry.
 * @param {Object} data The properties.
 * @private
 */
MetadataCache.prototype.mergeProperties_ = function(entry, data) {
  if (data === null) return;
  var properties = this.cache_[entry.toURL()].properties;
  for (var type in data) {
    if (data.hasOwnProperty(type) && !properties.hasOwnProperty(type)) {
      properties[type] = data[type];
      this.notifyObservers_(entry, type);
    }
  }
};

/**
 * Base class for metadata providers.
 * @constructor
 */
function MetadataProvider() {
}

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
MetadataProvider.prototype.supportsEntry = function(entry) { return false; };

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
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 */
MetadataProvider.prototype.fetch = function(entry, type, callback) {
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
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
FilesystemProvider.prototype.supportsEntry = function(entry) {
  return true;
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
FilesystemProvider.prototype.providesType = function(type) {
  return type === 'filesystem';
};

/**
 * @return {string} Unique provider id.
 */
FilesystemProvider.prototype.getId = function() { return 'filesystem'; };

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 */
FilesystemProvider.prototype.fetch = function(
    entry, type, callback) {
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

  entry.getMetadata(onMetadata.bind(null, entry), onError);
};

/**
 * Provider of drive metadata.
 * This provider returns the following objects:
 *     drive: { pinned, hosted, present, customIconUrl, etc. }
 *     thumbnail: { url, transform }
 *     streaming: { }
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @constructor
 */
function DriveProvider(volumeManager) {
  MetadataProvider.call(this);

  /**
   * @type {VolumeManagerWrapper}
   * @private
   */
  this.volumeManager_ = volumeManager;

  // We batch metadata fetches into single API call.
  this.entries_ = [];
  this.callbacks_ = [];
  this.scheduled_ = false;

  this.callApiBound_ = this.callApi_.bind(this);
}

DriveProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
DriveProvider.prototype.supportsEntry = function(entry) {
  var locationInfo = this.volumeManager_.getLocationInfo(entry);
  return locationInfo && locationInfo.isDriveBased;
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
DriveProvider.prototype.providesType = function(type) {
  return type === 'drive' || type === 'thumbnail' ||
      type === 'streaming' || type === 'media';
};

/**
 * @return {string} Unique provider id.
 */
DriveProvider.prototype.getId = function() { return 'drive'; };

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 */
DriveProvider.prototype.fetch = function(entry, type, callback) {
  this.entries_.push(entry);
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

  var entries = this.entries_;
  var callbacks = this.callbacks_;
  this.entries_ = [];
  this.callbacks_ = [];
  var self = this;

  var task = function(entry, callback) {
    // TODO(mtomasz): Make getDriveEntryProperties accept Entry instead of URL.
    var entryURL = entry.toURL();
    chrome.fileBrowserPrivate.getDriveEntryProperties(entryURL,
        function(properties) {
          callback(self.convert_(properties, entry));
        });
  };

  for (var i = 0; i < entries.length; i++)
    task(entries[i], callbacks[i]);
};

/**
 * @param {DriveEntryProperties} data Drive entry properties.
 * @param {Entry} entry File entry.
 * @return {boolean} True if the file is available offline.
 */
DriveProvider.isAvailableOffline = function(data, entry) {
  if (data.isPresent)
    return true;

  if (!data.isHosted)
    return false;

  // What's available offline? See the 'Web' column at:
  // http://support.google.com/drive/answer/1628467
  var subtype = FileType.getType(entry).subtype;
  return (subtype === 'doc' ||
          subtype === 'draw' ||
          subtype === 'sheet' ||
          subtype === 'slides');
};

/**
 * @param {DriveEntryProperties} data Drive entry properties.
 * @return {boolean} True if opening the file does not require downloading it
 *    via a metered connection.
 */
DriveProvider.isAvailableWhenMetered = function(data) {
  return data.isPresent || data.isHosted;
};

/**
 * Converts API metadata to internal format.
 * @param {Object} data Metadata from API call.
 * @param {Entry} entry File entry.
 * @return {Object} Metadata in internal format.
 * @private
 */
DriveProvider.prototype.convert_ = function(data, entry) {
  var result = {};
  result.drive = {
    present: data.isPresent,
    pinned: data.isPinned,
    hosted: data.isHosted,
    imageWidth: data.imageWidth,
    imageHeight: data.imageHeight,
    imageRotation: data.imageRotation,
    availableOffline: DriveProvider.isAvailableOffline(data, entry),
    availableWhenMetered: DriveProvider.isAvailableWhenMetered(data),
    customIconUrl: data.customIconUrl || '',
    contentMimeType: data.contentMimeType || '',
    sharedWithMe: data.sharedWithMe,
    shared: data.shared
  };

  if (!data.isPresent) {
    // Block the local fetch for drive files, which require downloading.
    result.thumbnail = {url: '', transform: null};
    result.media = {};
  }

  if ('thumbnailUrl' in data) {
    result.thumbnail = {
      url: data.thumbnailUrl,
      transform: null
    };
  }
  if (!data.isPresent) {
    // Indicate that the data is not available in local cache.
    // It used to have a field 'url' for streaming play, but it is
    // derprecated. See crbug.com/174560.
    result.streaming = {};
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
      'foreground/js/metadata/metadata_dispatcher.js';

  this.dispatcher_ = new SharedWorker(workerPath).port;
  this.dispatcher_.start();

  this.dispatcher_.onmessage = this.onMessage_.bind(this);
  this.dispatcher_.postMessage({verb: 'init'});

  // Initialization is not complete until the Worker sends back the
  // 'initialized' message.  See below.
  this.initialized_ = false;

  // Map from Entry.toURL() to callback.
  // Note that simultaneous requests for same url are handled in MetadataCache.
  this.callbacks_ = {};
}

ContentProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

/**
 * @param {Entry} entry The entry.
 * @return {boolean} Whether this provider supports the entry.
 */
ContentProvider.prototype.supportsEntry = function(entry) {
  return entry.toURL().match(this.urlFilter_);
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
ContentProvider.prototype.providesType = function(type) {
  return type === 'thumbnail' || type === 'fetchedMedia' || type === 'media';
};

/**
 * @return {string} Unique provider id.
 */
ContentProvider.prototype.getId = function() { return 'content'; };

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {string} type Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 */
ContentProvider.prototype.fetch = function(entry, type, callback) {
  if (entry.isDirectory) {
    callback({});
    return;
  }
  var entryURL = entry.toURL();
  this.callbacks_[entryURL] = callback;
  this.dispatcher_.postMessage({verb: 'request', arguments: [entryURL]});
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
  util.testSendMessage('worker-initialized');
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
 * @param {string} url File entry.
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
