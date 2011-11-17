// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} opt_workerPath path to the worker source JS file.
 */
function MetadataProvider(opt_workerPath) {
  this.cache_ = {};

  // Pass all URLs to the metadata reader until we have a correct filter.
  this.urlFilter = /.*/;

  if (!opt_workerPath) {
    var path = document.location.pathname;
    opt_workerPath = document.location.origin +
        path.substring(0, path.lastIndexOf('/') + 1) +
        'js/metadata_dispatcher.js';
  }

  this.dispatcher_ = new Worker(opt_workerPath);
  this.dispatcher_.onmessage = this.onMessage_.bind(this);
  this.dispatcher_.postMessage({verb: 'init'});
  // Initialization is not complete until the Worker sends back the
  // 'initialized' message.  See below.
}

MetadataProvider.prototype.fetch = function(url, callback) {
  var cacheValue = this.cache_[url];

  if (!cacheValue) {
    // This is the first time anyone's asked, go get it.
    if (url.match(this.urlFilter)) {
      this.cache_[url] = [callback];
      this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
      return;
    }
    // Cannot extract metadata for this file, return an empty map.
    setTimeout(function() { callback({}) }, 0);
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

  console.error('Unexpected metadata cache value:' + cacheValue);
};

MetadataProvider.prototype.reset = function(url) {
  if (this.cache_[url] instanceof Array) {
    console.error(
        'Abandoned ' + this.cache_[url].length + ' metadata subscribers',
        url);
  }
  delete this.cache_[url];
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
  var observers = this.cache_[url];
  if (!observers || !(observers instanceof Array)) {
    console.error('Missing or invalid metadata observers: ' + url + ': ' +
                  observers);
    return;
  }

  console.log('metadata result:', metadata);
  for (var i = 0; i < observers.length; i++) {
    observers[i](metadata);
  }

  this.cache_[url] = metadata;
};

MetadataProvider.prototype.onError_ = function(url, step, error, metadata) {
  console.warn('metadata: ' + url + ': ' + step + ': ' + error);
  this.onResult_(url, metadata || {});
};

MetadataProvider.prototype.onLog_ = function(arglist) {
  console.log.apply(console, ['metadata:'].concat(arglist));
};
