// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock of MetadataProvider.
 *
 * @param {string} type Type of metadata provided by the class.
 * @constructor
 */
function MockProvider(type) {
  MetadataProvider.call(this);
  this.type = type;
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
  return type === this.type;
};

MockProvider.prototype.getId = function() {
  return this.type;
};

MockProvider.prototype.fetch = function(entry, type, callback) {
  this.callbackPool.push(callback);
};

/**
 * Short hand for the metadataCache.get.
 *
 * @param {MetadataCache} meatadataCache Metadata cache.
 * @param {Array.<Entry>} entries Entries.
 * @param {string} type Metadata type.
 * @return {Promise} Promise to be fulfilled with the result metadata.
 */
function getMetadata(metadataCache, entries, type) {
  return new Promise(metadataCache.get.bind(metadataCache, entries, type));
};

/**
 * Short hand for the metadataCache.getLatest.
 * @param {MetadataCache} meatadataCache Metadata cache.
 * @param {Array.<Entry>} entries Entries.
 * @param {string} type Metadata type.
 * @return {Promise} Promise to be fulfilled with the result metadata.
 */
function getLatest(metadataCache, entries, type) {
  return new Promise(metadataCache.getLatest.bind(
      metadataCache, entries, type));
}

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
 * Confirms metadata is cached for the same entry.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testCache(callback) {
  var provider = new MockProvider('instrument');
  var metadataCache = new MetadataCache([provider]);
  var entry = new MockFileEntry('volumeId', '/music.txt');

  var metadataFromProviderPromise =
      getMetadata(metadataCache, [entry], 'instrument');
  var cachedBeforeFetchingPromise =
      getMetadata(metadataCache, [entry], 'instrument');
  assertEquals(1, provider.callbackPool.length);
  provider.callbackPool[0]({instrument: {name: 'banjo'}});
  var cachedAfterFethingPromise =
      getMetadata(metadataCache, [entry], 'instrument');

  // Provide should be called only once.
  assertEquals(1, provider.callbackPool.length);

  reportPromise(Promise.all([
    metadataFromProviderPromise,
    cachedBeforeFetchingPromise,
    cachedAfterFethingPromise
  ]).then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata[0]);
    assertDeepEquals([{name: 'banjo'}], metadata[1]);
    assertDeepEquals([{name: 'banjo'}], metadata[1]);
  }), callback);
}

/**
 * Confirms metadata is not cached for different entries.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testNoCacheForDifferentEntries(callback) {
  var provider = new MockProvider('instrument');
  var metadataCache = new MetadataCache([provider]);

  var entry1 = new MockFileEntry('volumeId', '/music1.txt');
  var entry2 = new MockFileEntry('volumeId', '/music2.txt');

  var entry1MetadataPromise =
      getMetadata(metadataCache, [entry1], 'instrument');
  var entry2MetadataPromise =
      getMetadata(metadataCache, [entry2], 'instrument');

  // Provide should be called for each entry.
  assertEquals(2, provider.callbackPool.length);

  provider.callbackPool[0]({instrument: {name: 'banjo'}});
  provider.callbackPool[1]({instrument: {name: 'fiddle'}});

  reportPromise(Promise.all([
    entry1MetadataPromise,
    entry2MetadataPromise
  ]).then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata[0]);
    assertDeepEquals([{name: 'fiddle'}], metadata[1]);
  }), callback);
}

/**
 * Confirms metadata is not cached for different entries.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testNoCacheForDifferentTypes(callback) {
  var providers = [
    new MockProvider('instrument'),
    new MockProvider('beat')
  ];
  var metadataCache = new MetadataCache(providers);

  var entry = new MockFileEntry('volumeId', '/music.txt');
  var instrumentMedatataPromise =
      getMetadata(metadataCache, [entry], 'instrument');
  var beatMetadataPromise = getMetadata(metadataCache, [entry], 'beat');
  assertEquals(1, providers[0].callbackPool.length);
  assertEquals(1, providers[1].callbackPool.length);

  providers[0].callbackPool[0]({instrument: {name: 'banjo'}});
  providers[1].callbackPool[0]({beat: {number: 2}});
  reportPromise(Promise.all([
    instrumentMedatataPromise,
    beatMetadataPromise
  ]).then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata[0]);
    assertDeepEquals([{number: 2}], metadata[1]);
  }), callback);
}

/**
 * Tests to call MetadataCache.get in verious order.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testGetDiffrentTypesInVeriousOrders(callback) {
  var providers = [
    new MockProvider('instrument'),
    new MockProvider('beat')
  ];
  var metadataCache = new MetadataCache(providers);

  var getAndCheckMetadata = function(entry, type, expected) {
    return getMetadata(metadataCache, [entry], type).then(function(metadata) {
      assertDeepEquals([expected], metadata);
    });
  };

  var entry1 = new MockFileEntry('volumeId', '/music1.txt');
  var promise1 = Promise.all([
    getAndCheckMetadata(entry1, 'instrument', {name: 'banjo'}),
    getAndCheckMetadata(entry1, 'beat', {number: 2}),
    getAndCheckMetadata(
        entry1,
        'instrument|beat',
        {instrument: {name: 'banjo'}, beat: {number: 2}})
  ]);
  assertEquals(1, providers[0].callbackPool.length);
  assertEquals(1, providers[1].callbackPool.length);

  var entry2 = new MockFileEntry('volumeId', '/music2.txt');
  var promise2 = Promise.all([
    getAndCheckMetadata(entry2, 'instrument', {name: 'banjo'}),
    getAndCheckMetadata(
        entry2,
        'instrument|beat',
        {instrument: {name: 'banjo'}, beat: {number: 2}}),
    getAndCheckMetadata(entry2, 'beat', {number: 2})
  ]);
  assertEquals(2, providers[0].callbackPool.length);
  assertEquals(2, providers[1].callbackPool.length);

  var entry3 = new MockFileEntry('volumeId', '/music3.txt');
  var promise3 = Promise.all([
    getAndCheckMetadata(
        entry3,
        'instrument|beat',
        {instrument: {name: 'banjo'}, beat: {number: 2}}),
    getAndCheckMetadata(entry3, 'instrument', {name: 'banjo'}),
    getAndCheckMetadata(entry3, 'beat', {number: 2})
  ]);
  assertEquals(3, providers[0].callbackPool.length);
  assertEquals(3, providers[1].callbackPool.length);

  for (var i = 0; i < providers[0].callbackPool.length; i++) {
    providers[0].callbackPool[i]({instrument: {name: 'banjo'}});
  }
  for (var i = 0; i < providers[1].callbackPool.length; i++) {
    providers[1].callbackPool[i]({beat: {number: 2}});
  }

  reportPromise(
      Promise.all([promise1, promise2, promise3]),
      callback);
}

/**
 * Tests MetadataCache.getCached.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testGetCached(callback) {
  var provider = new MockProvider('instrument');
  var metadataCache = new MetadataCache([provider]);

  var entry = new MockFileEntry('volumeId', '/music.txt');

  // Check the cache does exist before calling getMetadata.
  assertEquals(null, metadataCache.getCached(entry, 'instrument'));
  var promise = getMetadata(metadataCache, [entry], 'instrument');

  // Check the cache does exist after calling getMetadata but before receiving
  // metadata from a provider.
  assertEquals(null, metadataCache.getCached(entry, 'instrument'));

  provider.callbackPool[0]({instrument: {name: 'banjo'}});
  reportPromise(promise.then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata);
    assertDeepEquals(
        {name: 'banjo'},
        metadataCache.getCached(entry, 'instrument'));
  }), callback);
}

/**
 * Tests MetadataCache.getLatest.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testGetLatest(callback) {
  var provider = new MockProvider('instrument');
  var metadataCache = new MetadataCache([provider]);
  var entry = new MockFileEntry('volumeId', '/music.txt');

  var promise = getLatest(metadataCache, [entry], 'instrument');
  assertEquals(1, provider.callbackPool.length);
  provider.callbackPool[0]({instrument: {name: 'banjo'}});

  reportPromise(promise.then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata);
  }), callback);
};

/**
 * Tests that MetadataCache.getLatest ignore the existing cache.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testGetLatestToIgnoreCache(callback) {
  var provider = new MockProvider('instrument');
  var metadataCache = new MetadataCache([provider]);
  var entry = new MockFileEntry('volumeId', '/music.txt');

  var promise1 = getMetadata(metadataCache, [entry], 'instrument');
  assertEquals(1, provider.callbackPool.length);
  provider.callbackPool[0]({instrument: {name: 'banjo'}});
  assertDeepEquals(
      {name: 'banjo'}, metadataCache.getCached(entry, 'instrument'));
  var promise2 = getLatest(metadataCache, [entry], 'instrument');
  assertEquals(2, provider.callbackPool.length);
  assertDeepEquals(
      {name: 'banjo'}, metadataCache.getCached(entry, 'instrument'));
  provider.callbackPool[1]({instrument: {name: 'fiddle'}});
  assertDeepEquals(
      {name: 'fiddle'}, metadataCache.getCached(entry, 'instrument'));

  reportPromise(Promise.all([promise1, promise2]).then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata[0]);
    assertDeepEquals([{name: 'fiddle'}], metadata[1]);
  }), callback);
}

/**
 * Tests that the result of getLatest does not passed to the previous call of
 * getMetadata.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testGetLatestAndPreviousCall(callback) {
  var provider = new MockProvider('instrument');
  var metadataCache = new MetadataCache([provider]);
  var entry = new MockFileEntry('volumeId', '/music.txt');

  var promise1 = getMetadata(metadataCache, [entry], 'instrument');
  assertEquals(1, provider.callbackPool.length);
  var promise2 = getLatest(metadataCache, [entry], 'instrument');
  assertEquals(2, provider.callbackPool.length);

  provider.callbackPool[1]({instrument: {name: 'fiddle'}});
  provider.callbackPool[0]({instrument: {name: 'banjo'}});
  assertDeepEquals(
      {name: 'banjo'}, metadataCache.getCached(entry, 'instrument'));

  reportPromise(Promise.all([promise1, promise2]).then(function(metadata) {
    assertDeepEquals([{name: 'banjo'}], metadata[0]);
    assertDeepEquals([{name: 'fiddle'}], metadata[1]);
  }), callback);
}

/**
 * Tests the MetadataCache#clear method.
 *
 * @param {function(boolean=)} callback Callback to be called when test
 *     completes. If the test fails, true is passed to the function.
 */
function testClear(callback) {
  var providers = [
    new MockProvider('instrument'),
    new MockProvider('beat')
  ];
  var metadataCache = new MetadataCache(providers);
  var entry = new MockFileEntry('volumeId', '/music.txt');

  var promise1 = getMetadata(metadataCache, [entry], 'instrument');
  var promise2 = getMetadata(metadataCache, [entry], 'beat');
  assertEquals(1, providers[0].callbackPool.length);
  assertEquals(1, providers[1].callbackPool.length);
  providers[0].callbackPool[0]({instrument: {name: 'banjo'}});
  providers[1].callbackPool[0]({beat: {number: 2}});
  assertDeepEquals(
      {name: 'banjo'}, metadataCache.getCached(entry, 'instrument'));
  assertDeepEquals({number: 2}, metadataCache.getCached(entry, 'beat'));

  metadataCache.clear([entry], 'instrument');
  assertEquals(
      null, metadataCache.getCached(entry, 'instrument'));
  assertDeepEquals({number: 2}, metadataCache.getCached(entry, 'beat'));

  var promise3 = getMetadata(metadataCache, [entry], 'instrument');
  assertEquals(2, providers[0].callbackPool.length);
  providers[0].callbackPool[1]({instrument: {name: 'fiddle'}});
  assertDeepEquals(
      {name: 'fiddle'}, metadataCache.getCached(entry, 'instrument'));

  reportPromise(Promise.all([promise1, promise2, promise3]).then(
      function(metadata) {
        assertDeepEquals([{name: 'banjo'}], metadata[0]);
        assertDeepEquals([{number: 2}], metadata[1]);
        assertDeepEquals([{name: 'fiddle'}], metadata[2]);
      }), callback);
}
