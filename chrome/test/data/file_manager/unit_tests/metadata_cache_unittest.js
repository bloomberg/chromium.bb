// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var metadataCache;
var provider;

/**
 * Mock of MetadataProvider.
 * @constructor
 */
function MockProvider() {
  MetadataProvider.call(this);
  this.callbackPool = [];
  Object.freeze(this);
}

MockProvider.prototype = {
  __proto__: MetadataProvider.prototype
};

MockProvider.prototype.supportsEntry = function(entry) {
  return true;
};

MockProvider.prototype.providesType = function(type) {
  return type === 'stub';
};

MockProvider.prototype.getId = function() {
  return 'stub';
};

MockProvider.prototype.fetch = function(entry, type, callback) {
  this.callbackPool.push(callback);
};

/**
 * Short hand for the metadataCache.get.
 *
 * @param {Array.<Entry>} entries Entries.
 * @param {string} type Metadata type.
 * @return {Promise} Promise to be fulfilled with the result metadata.
 */
function getMetadata(entries, type) {
  return new Promise(metadataCache.get.bind(metadataCache, entries, type));
};

/**
 * Invokes a callback function depending on the result of promise.
 *
 * @param {Promise} promise Promise.
 * @param {function(boolean)} calllback Callback function. True is passed if the
 * test failed.
 */
function reportPromise(promise, callback) {
  promise.then(function() {
    callback(/* error */ false);
  }, function(error) {
    console.error(error.stack || error);
    callback(/* error */ true);
  });
}

/**
 * Constructs the metadata cache and its provider.
 */
function setUp() {
  provider = new MockProvider();
  metadataCache = new MetadataCache([provider]);
}

/**
 * Confirms metadata is cached for the same entry.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testCached(callback) {
  var entry = new MockFileEntry('volumeId', '/banjo.txt');

  var promises = [];
  var firstPromise = getMetadata([entry], 'stub');
  var cachedBeforeFetchingPromise =  getMetadata([entry], 'stub');
  provider.callbackPool[0]({stub: {value: 'banjo'}});
  var cachedAfterFethingPromise = getMetadata([entry], 'stub');

  // Provide should be called only once.
  assertEquals(1, provider.callbackPool.length);

  reportPromise(Promise.all([
    firstPromise,
    cachedBeforeFetchingPromise,
    cachedAfterFethingPromise
  ]).then(function(metadata) {
    assertDeepEquals([{value: 'banjo'}], metadata[0]);
    assertDeepEquals([{value: 'banjo'}], metadata[1]);
    assertDeepEquals([{value: 'banjo'}], metadata[1]);
  }), callback);
}

/**
 * Confirms metadata is not cached for different entries.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testNotCached(callback) {
  var entry1 = new MockFileEntry('volumeId', '/banjo.txt');
  var entry2 = new MockFileEntry('volumeId', '/fiddle.txt');

  var firstPromise = getMetadata([entry1], 'stub');
  var anotherPromise = getMetadata([entry2], 'stub');

  // Provide should be called for each entry.
  assertEquals(2, provider.callbackPool.length);

  provider.callbackPool[0]({stub: {value: 'banjo'}});
  provider.callbackPool[1]({stub: {value: 'fiddle'}});

  reportPromise(Promise.all([
    firstPromise,
    anotherPromise
  ]).then(function(metadata) {
    assertDeepEquals([{value: 'banjo'}], metadata[0]);
    assertDeepEquals([{value: 'fiddle'}], metadata[1]);
  }), callback);
}
