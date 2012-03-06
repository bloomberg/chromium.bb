// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A base class for metadata providers. Implementing in-memory caching.
 */
function MetadataCache() {
  this.cache_ = {};
}

MetadataCache.prototype.isSupported = function(url) {
  return true;
};

MetadataCache.prototype.doFetch = function(url) {
  throw new Error('MetadataCache.doFetch not implemented');
};

MetadataCache.prototype.fetch = function(url, callback) {
  var cacheValue = this.cache_[url];

  if (!cacheValue) {
    // This is the first time anyone's asked, go get it.
    if (this.isSupported(url)) {
      this.cache_[url] = [callback];
      this.doFetch(url);
    } else {
      // Fetch failed, return an empty map, no caching needed.
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
    setTimeout(function() { callback(cacheValue) }, 0);
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
 * Create metadata provider for local files.
 */
function MetadataProvider() {
  MetadataCache.call(this);

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
}

MetadataProvider.prototype = { __proto__: MetadataCache.prototype };

MetadataProvider.prototype.isSupported = function(url) {
  return url.match(this.urlFilter);
};

MetadataProvider.prototype.doFetch = function(url) {
  this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
};

/**
 * Dispatch a message from a metadata reader to the appropriate on* method.
 */
MetadataProvider.prototype.onMessage_ = function(event) {
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
MetadataProvider.prototype.onInitialized_ = function(regexp) {
  this.urlFilter = regexp;
};

MetadataProvider.prototype.onResult_ = function(url, metadata) {
  this.processResult(url, metadata);
};

MetadataProvider.prototype.onError_ = function(url, step, error, metadata) {
  console.warn('metadata: ' + url + ': ' + step + ': ' + error);
  this.processResult(url, metadata || {});
};

MetadataProvider.prototype.onLog_ = function(arglist) {
  console.log.apply(console, ['metadata:'].concat(arglist));
};

/**
 * Create a metadata provider for GData files.
 *
 * Instead of reading the file contents it gets the metadata from GData API.
 */
function GDataMetadataProvider() {
  MetadataCache.call(this);
}

GDataMetadataProvider.prototype = { __proto__: MetadataCache.prototype };

GDataMetadataProvider.prototype.doFetch = function(url) {
  function convertMetadata(result) {
    return {
      thumbnailURL: result.thumbnailUrl,
      thumbnailOnly: true,  // Temporary, unless we learn to fetch the content.
      fileSize: 0  // TODO(kaznacheev) Get the correct size from File API.
    }
  }

  chrome.fileBrowserPrivate.getGDataFileProperties([url],
    function(results) {
      this.processResult(url, convertMetadata(results[0]));
    }.bind(this));
};
