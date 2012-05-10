// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * MetadataCache is a map from url to an object containing properties.
 * Properties are divided by types, and all properties of one type are accessed
 * at once.
 * Some of the properties:
 * {
 *   filesystem: size, modificationTime, icon
 *   internal: presence
 *   gdata: pinned, present, hosted, editUrl, contentUrl, availableOffline
 *   thumbnail: url, transform
 *   media: artist, album, title
 * }
 *
 * Typical usages:
 * {
 *   cache.get([entry1, entry2], 'gdata', function(gdata) {
 *     if (gdata[0].pinned && gdata[1].pinned) alert("They are both pinned!");
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
 * }
 *
 * TODO(dgozman): eviction.
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
}

/**
 * Observer type: it will be notified if the url changed is exactlt the same
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
 * @return {MetadataCache!} The cache with all providers.
 */
MetadataCache.createFull = function() {
  var cache = new MetadataCache();
  cache.providers_.push(new FilesystemProvider());
  cache.providers_.push(new GDataProvider());
  return cache;
};

/**
 * Fetches the metadata, puts it in the cache, and passes to callback.
 * If required metadata is already in the cache, does not fetch it again.
 * @param {string|Entry|Array.<string|Entry>} items The list of entries or
 *     file urls. May be just a single item.
 * @param {string} type The metadata type.
 * @param {Function(Object)} callback The metadata is passed to callback.
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

  function onOneItem(index, value) {
    result[index] = value;
    remaining--;
    if (remaining == 0) {
      this.endBatchUpdates();
      if (callback) setTimeout(callback, 0, result);
    }
  }

  for (var index = 0; index < items.length; index++) {
    result.push(null);
    this.getOne(items[index], type, onOneItem.bind(this, index));
  }
};

/**
 * Fetches the metadata for one Entry/FileUrl. See comments to |get|.
 * @param {Entry|string} item The entry or url.
 * @param {string} type Metadata type.
 * @param {Function(Object)} callback The callback.
 */
MetadataCache.prototype.getOne = function(item, type, callback) {
  var url = this.itemToUrl_(item);

  // Passing entry to fetchers may save one round-trip to APIs.
  var fsEntry = item === url ? null : item;
  callback = callback || function() {};

  if (!(url in this.cache_))
    this.cache_[url] = this.createEmptyEntry_();

  var entry = this.cache_[url];

  if (type in entry.properties) {
    callback(entry.properties[type]);
    return;
  }

  this.startBatchUpdates();
  var providers = this.providers_.slice();
  var currentProvider;
  var self = this;

  function onFetched() {
    if (type in entry.properties) {
      self.endBatchUpdates();
      // Got properties from provider.
      callback(entry.properties[type]);
    } else {
      tryNextProvider();
    }
  }

  function onProviderProperties(properties) {
    var id = currentProvider.getId();
    var fetchedCallbacks = entry[id].callbacks;
    delete entry[id].callbacks;
    entry[id].time = new Date();
    self.mergeProperties_(url, properties);

    for (var index = 0; index < fetchedCallbacks.length; index++) {
      fetchedCallbacks[index]();
    }
  }

  function queryProvider() {
    var id = currentProvider.getId();
    if ('callbacks' in entry[id]) {
      // We are querying this provider now.
      entry[id].callbacks.push(onFetched);
    } else {
      entry[id].callbacks = [onFetched];
      currentProvider.fetch(url, type, onProviderProperties, fsEntry);
    }
  }

  function tryNextProvider() {
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
  }

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
  if (!(items instanceof Array))
    items = [items];

  this.startBatchUpdates();
  for (var index = 0; index < items.length; index++) {
    var url = this.itemToUrl_(items[index]);
    if (!(url in this.cache_))
      this.cache_[url] = this.createEmptyEntry_();
    this.cache_[url].properties[type] = values[index];
    this.notifyObservers_(url, type);
  }
  this.endBatchUpdates();
};

/**
 * Clears the cached metadata values.
 * @param {string|Entry|Array.<string|Entry>} items The list of entries or
 *     file urls. May be just a single item.
 * @param {string} type The metadata type.
 */
MetadataCache.prototype.clear = function(items, type) {
  if (!(items instanceof Array))
    items = [items];

  for (var index = 0; index < items.length; index++) {
    var url = this.itemToUrl_(items[index]);
    if (url in this.cache_)
      delete this.cache_[url].properties[type];
  }
};

/**
 * Adds an observer, which will be notified when metadata changes.
 * @param {string|Entry} item The root item to look at.
 * @param {number} relation This defines, which items will trigger the observer.
 *     See comments to |MetadataCache.EXACT| and others.
 * @param {string} type The metadata type.
 * @param {Function(Array.<string>, Array.<Object>)} observer List of file urls
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
};

/**
 * End batch updates. Notifies observers if all nested updates are finished.
 */
MetadataCache.prototype.endBatchUpdates = function() {
  this.batchCount_--;
  if (this.batchCount_ != 0) return;
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
 * Converts Entry or file url to url.
 * @param {string|Entry} item Item to convert.
 * @return {string} File url.
 * @private
 */
MetadataCache.prototype.itemToUrl_ = function(item) {
  if (typeof(item) == 'string')
    return item;
  else
    return item.toURL();
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
    if (data.hasOwnProperty(type)) {
      properties[type] = data[type];
      this.notifyObservers_(url, type);
    }
  }
};


/**
 * Base class for metadata providers.
 * @constructor
 * TODO(dgozman): rename to MetadataProvider (now it conflicts with another
 *     entity).
 */
function MetadataProvider2() {
}

/**
 * @param {string} url The url.
 * @return {boolean} Whether this provider supports the url.
 */
MetadataProvider2.prototype.supportsUrl = function(url) { return false; };

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
MetadataProvider2.prototype.providesType = function(type) { return false; };

/**
 * @return {string} Unique provider id.
 */
MetadataProvider2.prototype.getId = function() { return ''; };

/**
 * Fetches the metadata. It's suggested to return all the metadata this provider
 * can fetch at once.
 * @param {string} url File url.
 * @param {string} type Requested metadata type.
 * @param {Function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 * @param {Entry=} opt_entry The file entry if present.
 */
MetadataProvider2.prototype.fetch = function(url, type, callback, opt_entry) {
  throw new Error('Default metadata provider cannot fetch.');
};


/**
 * Provider of filesystem metadata.
 * This provider returns the following objects:
 * filesystem: {
 *   size;
 *   modificationTime;
 *   icon - string describing icon type;
 * }
 * @constructor
 */
function FilesystemProvider() {
  MetadataProvider2.call(this, 'filesystem');
}

FilesystemProvider.prototype = {
  __proto__: MetadataProvider2.prototype
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
 * @param {Function(Object)} callback Callback expects a map from metadata type
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
    webkitResolveLocalFileSystemURL(url, onEntry, onError);
};

/**
 * Provider of gdata metadata.
 * This provider returns the following objects:
 *     gdata: { pinned, hosted, present, dirty, editUrl, contentUrl }
 *     thumbnail: { url, transform }
 * @constructor
 */
function GDataProvider() {
  MetadataProvider2.call(this, 'gdata');

  // We batch metadata fetches into single API call.
  this.urls_ = [];
  this.callbacks_ = [];
  this.scheduled_ = false;

  this.callApiBound_ = this.callApi_.bind(this);
}

GDataProvider.prototype = {
  __proto__: MetadataProvider2.prototype
};

/**
 * Pattern for urls supported by GDataProvider.
 */
GDataProvider.URL_PATTERN =
    new RegExp('^filesystem:[^/]*://[^/]*/[^/]*/drive/(.*)');

/**
 * @param {string} url The url.
 * @return {boolean} Whether this provider supports the url.
 */
GDataProvider.prototype.supportsUrl = function(url) {
  return GDataProvider.URL_PATTERN.test(url);
};

/**
 * @param {string} type The metadata type.
 * @return {boolean} Whether this provider provides this metadata.
 */
GDataProvider.prototype.providesType = function(type) {
  return type == 'gdata' || type == 'thumbnail';
};

/**
 * @return {string} Unique provider id.
 */
GDataProvider.prototype.getId = function() { return 'gdata'; };

/**
 * Fetches the metadata.
 * @param {string} url File url.
 * @param {string} type Requested metadata type.
 * @param {Function(Object)} callback Callback expects a map from metadata type
 *     to metadata value.
 * @param {Entry=} opt_entry The file entry if present.
 */
GDataProvider.prototype.fetch = function(url, type, callback, opt_entry) {
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
GDataProvider.prototype.callApi_ = function() {
  this.scheduled_ = false;

  var urls = this.urls_;
  var callbacks = this.callbacks_;
  this.urls_ = [];
  this.callbacks_ = [];
  var self = this;

  chrome.fileBrowserPrivate.getGDataFileProperties(urls, function(props) {
    for (var index = 0; index < urls.length; index++) {
      callbacks[index](self.convert_(props[index]));
    }
  });
};

/**
 * Pattern for Docs urls that are available in the offline mode.
 */
GDataProvider.OFFLINE_DOCS_REGEXP = /\/(document|spreadsheet)\//;

/**
 * @param {GDataFileProperties} data GData file properties.
 * @return {boolean} True if the file is available offline.
 */
GDataProvider.isAvailableOffline = function(data) {
  return data.isPresent ||
      (data.isHosted && GDataProvider.OFFLINE_DOCS_REGEXP.test(data.editUrl));
};

/**
 * Converts API metadata to internal format.
 * @param {Object} data Metadata from API call.
 * @return {Object} Metadata in internal format.
 * @private
 */
GDataProvider.prototype.convert_ = function(data) {
  var result = {};
  result.gdata = {
    present: data.isPresent,
    pinned: data.isPinned,
    hosted: data.isHosted,
    dirty: data.isDirty,
    availableOffline: GDataProvider.isAvailableOffline(data),
    contentUrl: (data.contentUrl || '').replace(/\?.*$/gi, ''),
    editUrl: data.editUrl || ''
  };
  if ('thumbnailUrl' in data) {
    result.thumbnail = {
      url: data.thumbnailUrl,
      transform: ''
    };
  }
  return result;
};
