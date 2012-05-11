// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A base class for metadata provider. Implementing in-memory caching.
 */
function MetadataCache() {
  this.cache_ = {};
}

/**
 * @param {string} url
 * @param {Object} opt_param Opaque parameter, used by the overriders.
 */
MetadataCache.prototype.doFetch = function(url, opt_param) {
  throw new Error('MetadataCache.doFetch not implemented');
};

/**
 * @param {string} url
 * @param {function(Object)} callback
 * @param {Object} opt_forceParam If defined, forces doFetch call regardless of
 *   the cache state and is passed to doFetch.
 */
MetadataCache.prototype.fetch = function(url, callback, opt_forceParam) {
  var entry = this.cache_[url];

  if (!entry) {
    entry = this.cache_[url] = {};
  }

  var scheduleFetch = function() {
    if (entry.pending) {
      entry.pending.push(scheduleFetch);
    } else if (this.doFetch(url, opt_forceParam)) {
      entry.pending = [callback];
    } else {
      entry.result = entry.result || {};
      setTimeout(callback, 0, entry.result);
    }
  }.bind(this);

  if (entry.pending) {
    // Something is already pending, add to the list of callbacks.
    entry.pending.push(opt_forceParam ? scheduleFetch : callback);
    return;
  }

  if (!entry.result || opt_forceParam) {
    scheduleFetch();
    return;
  }

  setTimeout(callback, 0, entry.result);
};

MetadataCache.prototype.processResult = function(url, result) {
  console.log('metadata result:', result);

  var entry = this.cache_[url];
  if (!entry || ! entry.pending) {
    console.error('Missing or invalid metadata observers: ' + url + ': ',
                  entry);
    return;
  }

  if (!entry.result) {
    entry.result = result;
  } else {
    // Merge the new result into existing one.
    for (var key in result) {
      if (result.hasOwnProperty(key))
        entry.result[key] = result[key];
    }
  }

  var callbacks = entry.pending;
  delete entry.pending;
  while(callbacks.length) {
    callbacks.shift()(entry.result);
  }
};


MetadataCache.prototype.reset = function(url) {
  var entry = this.cache_[url];
  if (entry && entry.pending) {
    console.error(
        'Abandoned ' + entry.pending.length + ' metadata subscribers',
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
      remote: true,
      thumbnailURL: result.thumbnailUrl,
      streamingURL: result.isPresent ?
                        null :  // Do not stream if the file is cached.
                        (result.contentUrl || '').replace(/\?.*$/gi, ''),
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

  var callback = this.processResult.bind(this);

  // TODO(kaznacheev): We use 'gdata' literal here because
  // DirectoryModel.GDATA_DIRECTORY definition might be not available.
  // Consider extracting this definition into some common js file.
  this.gdata_ = new GDataMetadataFetcher(callback, rootUrl + 'drive' + '/'),
  this.local_ = new LocalMetadataFetcher(callback, rootUrl);

  this.fetchers_ = [this.gdata_, this.local_];
}

MetadataProvider.prototype = { __proto__: MetadataCache.prototype };

MetadataProvider.prototype.isInitialized = function() {
  for (var i = 0; i != this.fetchers_.length; i++) {
    if (!this.fetchers_[i].isInitialized())
      return false;
  }
  return true;
};

MetadataProvider.prototype.doFetch = function(url, opt_fetchers) {
  var fetchers = opt_fetchers || this.fetchers_;
  for (var i = 0; i != fetchers.length; i++) {
    var fetcher = fetchers[i];
    if (fetcher.supports(url)) {
      fetcher.doFetch(url);
      return true;
    }
  }
  return false;
};

/**
 * Force fetches metadata from a local file.
 * Expensive to call, should be used only if local metadata is a must have.
 */
MetadataProvider.prototype.fetchLocal = function(url, callback) {
  this.fetch(url, function(metadata) {
    if (metadata.remote) {
      delete metadata.remote;
      this.fetch(url, callback, [this.local_]);
    } else {
      callback(metadata);
    }
  }.bind(this));
};
