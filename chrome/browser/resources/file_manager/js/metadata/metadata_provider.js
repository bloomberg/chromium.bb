// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A base class for metadata provider. Implementing in-memory caching.
 */
function MetadataCache() {
  this.cache_ = {};
}

MetadataCache.prototype.doFetch = function(url) {
  throw new Error('MetadataCache.doFetch not implemented');
};

MetadataCache.prototype.fetch = function(url, callback) {
  var cacheValue = this.cache_[url];

  if (!cacheValue) {
    // This is the first time anyone has asked, go get it.
    this.cache_[url] = [callback];
    if (!this.doFetch(url)) {
      // Fetch failed, return an empty map, no caching needed.
      delete this.cache_[url];
      setTimeout(callback, 0, {});
    }
    return;
  }

  if (cacheValue instanceof Array) {
    // Something is already pending, add to the list of observers.
    cacheValue.push(callback);
    return;
  }

  if (cacheValue instanceof Object) {
    // We already know the answer, let the caller know in a fresh call stack.
    setTimeout(callback, 0, cacheValue);
    return;
  }

  console.error('Unexpected metadata cache value:', cacheValue);
};

MetadataCache.prototype.processResult = function(url, result) {
  console.log('metadata result:', result);

  var observers = this.cache_[url];
  if (!observers || !(observers instanceof Array)) {
    console.error('Missing or invalid metadata observers: ' + url + ': ',
                  observers);
    return;
  }

  for (var i = 0; i < observers.length; i++) {
    observers[i](result);
  }

  this.cache_[url] = result;
};


MetadataCache.prototype.reset = function(url) {
  if (this.cache_[url] instanceof Array) {
    console.error(
        'Abandoned ' + this.cache_[url].length + ' metadata subscribers',
        url);
  }
  delete this.cache_[url];
};

/**
 * Base class for metadata fetchers.
 *
 * @param {function(string, Object)} resultCallback
 * @param {string} urlPrefix.
 */
function MetadataFetcher(resultCallback, urlPrefix) {
  this.resultCallback_ = resultCallback;
  this.urlPrefix_ = urlPrefix;
}

MetadataFetcher.prototype.processResult = function(url, metadata) {
  this.resultCallback_(url, metadata);
};

MetadataFetcher.prototype.isInitialized = function() {
  return true;
};

MetadataFetcher.prototype.supports = function(url) {
  return url.indexOf(this.urlPrefix_) == 0;
};

MetadataFetcher.prototype.doFetch = function(url) {
  console.error('MetadataFetcher.doFetch not implemented');
};

/**
 * Create metadata cache for local files.
 */
function LocalMetadataFetcher(resultCallback, rootUrl) {
  MetadataFetcher.apply(this, arguments);

  // Pass all URLs to the metadata reader until we have a correct filter.
  this.urlFilter = /.*/;

  var path = document.location.pathname;
  var workerPath = document.location.origin +
      path.substring(0, path.lastIndexOf('/') + 1) +
      'js/metadata/metadata_dispatcher.js';

  this.dispatcher_ = new Worker(workerPath);
  this.dispatcher_.onmessage = this.onMessage_.bind(this);
  this.dispatcher_.postMessage({verb: 'init'});
  // Initialization is not complete until the Worker sends back the
  // 'initialized' message.  See below.
  this.initialized_ = false;
}

LocalMetadataFetcher.prototype = { __proto__: MetadataFetcher.prototype };

LocalMetadataFetcher.prototype.supports = function(url) {
  return MetadataFetcher.prototype.supports.apply(this, arguments) &&
      url.match(this.urlFilter);
};

LocalMetadataFetcher.prototype.doFetch = function(url) {
  this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
};

LocalMetadataFetcher.prototype.isInitialized = function() {
  return this.initialized_;
};

/**
 * Dispatch a message from a metadata reader to the appropriate on* method.
 */
LocalMetadataFetcher.prototype.onMessage_ = function(event) {
  var data = event.data;

  var methodName =
      'on' + data.verb.substr(0, 1).toUpperCase() + data.verb.substr(1) + '_';

  if (!(methodName in this)) {
    console.log('Unknown message from metadata reader: ' + data.verb, data);
    return;
  }

  this[methodName].apply(this, data.arguments);
};

/**
 * Handles the 'initialized' message from the metadata reader Worker.
 */
LocalMetadataFetcher.prototype.onInitialized_ = function(regexp) {
  this.urlFilter = regexp;

  // Tests can monitor for this state with
  // ExtensionTestMessageListener listener("worker-initialized");
  // ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Automated tests need to wait for this, otherwise we crash in
  // browser_test cleanup because the worker process still has
  // URL requests in-flight.
  chrome.test.sendMessage('worker-initialized');
  this.initialized_ = true;
};

LocalMetadataFetcher.prototype.onResult_ = function(url, metadata) {
  this.processResult(url, metadata);
};

LocalMetadataFetcher.prototype.onError_ = function(url, step, error, metadata) {
  console.warn('metadata: ' + url + ': ' + step + ': ' + error);
  this.processResult(url, metadata || {});
};

LocalMetadataFetcher.prototype.onLog_ = function(arglist) {
  console.log.apply(console, ['metadata:'].concat(arglist));
};

/**
 * Create a metadata fetcher for GData files.
 *
 * Instead of reading the file contents it gets the metadata from GData API.
 */
function GDataMetadataFetcher(resultCallback, rootUrl) {
  MetadataFetcher.apply(this, arguments);
}

GDataMetadataFetcher.prototype = { __proto__: MetadataFetcher.prototype };

GDataMetadataFetcher.prototype.doFetch = function(url) {
  function convertMetadata(result) {
    return {
      thumbnailURL: result.thumbnailUrl,
      contentURL: (result.contentUrl || '').replace(/\?.*$/gi, ''),
      fileSize: 0  // TODO(kaznacheev) Get the correct size from File API.
    }
  }

  chrome.fileBrowserPrivate.getGDataFileProperties([url],
    function(results) {
      this.processResult(url, convertMetadata(results[0]));
    }.bind(this));
};

/**
 * Create universal metadata provider.
 *
 * @param {string} rootUrl The file system root url.
 * @constructor
 */
function MetadataProvider(rootUrl) {
  MetadataCache.call(this);

  var resultCallback = this.processResult.bind(this);

  // TODO(kaznacheev): We use 'gdata' literal here because
  // DirectoryModel.GDATA_DIRECTORY definition might be not available.
  // Consider extracting this definition into some common js file.
  this.fetchers_ = [
    new GDataMetadataFetcher(resultCallback, rootUrl + 'gdata' + '/'),
    new LocalMetadataFetcher(resultCallback, rootUrl)
  ];
}

MetadataProvider.prototype = { __proto__: MetadataCache.prototype };

MetadataProvider.prototype.isInitialized = function() {
  for (var i = 0; i != this.fetchers_.length; i++) {
    if (!this.fetchers_[i].isInitialized())
      return false;
  }
  return true;
};

MetadataProvider.prototype.doFetch = function(url) {
  for (var i = 0; i != this.fetchers_.length; i++) {
    var fetcher = this.fetchers_[i];
    if (fetcher.supports(url)) {
      fetcher.doFetch(url);
      return true;
    }
  }
  return false;
};
