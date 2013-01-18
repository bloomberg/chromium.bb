// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var overallTestStartTime = Date.now();
var kUseIndex = true;
var kDontUseIndex = false;
var kReadKeysOnly = true;
var kReadDataToo = false;
var kWriteToo = true;
var kDontWrite = false;
var kWriteSameStore = true;
var kWriteDifferentStore = false;
var kPlaceholderArg = false;
var kDontRead = false;
var kAlternateWithReads = true;

var tests = [
// Create a single small item in a single object store, then delete everything.
  [testCreateAndDeleteDatabase,  1,    1,    1],
// Create many small items in a single object store, then delete everything.
  [testCreateAndDeleteDatabase,  1000,  1,    1],
// Create a single small item in many object stores, then delete everything.
  [testCreateAndDeleteDatabase,  1,    1000,  1],
// Create many large items in a single object store, then delete everything.
  [testCreateAndDeleteDatabase,  1000,    1,  10000],
// Create a single small item in a single object store.
  [testCreateKeysInStores, 1,     1,    1],
// Create many small items in a single object store.
  [testCreateKeysInStores, 1000,   1,    1],
// Create a single small item in many object stores.
  [testCreateKeysInStores, 1,     1000,  1],
// Create many large items in a single object store.
  [testCreateKeysInStores, 1000,   1,    10000],
// Read one item per transaction.
  [testRandomReadsAndWrites, 1000, 1, 0, 1000, kDontUseIndex],
// Read a few random items in each of many transactions.
  [testRandomReadsAndWrites, 1000,  5,    0,  100, kDontUseIndex],
// Read many random items in each of a few transactions.
  [testRandomReadsAndWrites, 1000,  500,   0,  5,  kDontUseIndex],
// Read many random items in each of a few transactions, in a large store.
  [testRandomReadsAndWrites, 10000,  500,   0,  5,  kDontUseIndex],
// Read a few random items from an index, in each of many transactions.
  [testRandomReadsAndWrites, 1000,  5,    0,  100, kUseIndex],
// Read many random items from an index, in each of a few transactions.
  [testRandomReadsAndWrites, 1000,  500,   0,  5,  kUseIndex],
// Read many random items from an index, in each of a few transactions, in a
// large store.
  [testRandomReadsAndWrites, 10000,  500,   0,  5,  kUseIndex],
// Read and write a few random items in each of many transactions.
  [testRandomReadsAndWrites, 1000,  5,    5,  50, kDontUseIndex],
// Read and write a few random items, reading from an index, in each of many
// transactions.
  [testRandomReadsAndWrites, 1000,  5,    5,  50, kUseIndex],
// Read a long, contiguous sequence of an object store via a cursor.
  [testCursorReadsAndRandomWrites, kReadDataToo, kDontUseIndex, kDontWrite,
   kPlaceholderArg],
// Read a sequence of an object store via a cursor, writing
// transformed values into another.
  [testCursorReadsAndRandomWrites, kReadDataToo, kDontUseIndex, kWriteToo,
   kWriteDifferentStore],
// Read a sequence of an object store via a cursor, writing
// transformed values into another.
  [testCursorReadsAndRandomWrites, kReadDataToo, kDontUseIndex, kWriteToo,
   kWriteSameStore],
// Read a sequence of an index into an object store via a cursor.
  [testCursorReadsAndRandomWrites, kReadDataToo, kUseIndex, kDontWrite,
   kPlaceholderArg],
// Read a sequence of an index into an object store via a key cursor.
  [testCursorReadsAndRandomWrites, kReadKeysOnly, kUseIndex, kDontWrite,
   kPlaceholderArg],
// Make batches of random writes into a store, triggered by periodic setTimeout
// calls.
  [testSporadicWrites, 5, 0, kDontRead],
// Make large batches of random writes into a store, triggered by periodic
// setTimeout calls.
  [testSporadicWrites, 50, 0, kDontRead],
// Make batches of random writes into a store with many indices, triggered by
// periodic setTimeout calls.
  [testSporadicWrites, 5, 10, kDontRead],
// Make large batches of random writes into a store with many indices, triggered
// by periodic setTimeout calls.
  [testSporadicWrites, 50, 10, kDontRead],
// Make batches of random writes into a store, triggered by periodic setTimeout
// calls.  Intersperse read transactions to test read-write lock conflicts.
  [testSporadicWrites, 5, 0, kAlternateWithReads],
// Make large batches of random writes into a store, triggered by periodic
// setTimeout calls.  Intersperse read transactions to test read-write lock
// conflicts.
  [testSporadicWrites, 50, 0, kAlternateWithReads],
// Make a small bunch of batches of reads of the same keys from an object store.
  [testReadCache, 10, kDontUseIndex],
// Make a bunch of batches of reads of the same keys from an index.
  [testReadCache, 50, kUseIndex],
// Make a small bunch of batches of reads of the same keys from an object store.
  [testReadCache, 10, kDontUseIndex],
// Make a bunch of batches of reads of the same keys from an index.
  [testReadCache, 50, kUseIndex],
// Create and delete an index on a store that already contains data [produces
// a timing result for each of creation and deletion].
  [testCreateAndDeleteIndex, 5000],
// Walk through multiple cursors into the same object store, round-robin, until
// you've reached the end of each of them.
  [testWalkingMultipleCursors, 5],
// Walk through many cursors into the same object store, round-robin, until
// you've reached the end of each of them.
  [testWalkingMultipleCursors, 50],
];

var currentTest = 0;

function test() {
  runNextTest();
}

function runNextTest() {
  var filter = window.location.hash.slice(1);
  var test, f;
  while (currentTest < tests.length) {
    test = tests[currentTest];
    f = test.shift();
    if (!filter || f.name == filter)
      break;
    ++currentTest;
  }

  if (currentTest < tests.length) {
    test.push(runNextTest);
    f.apply(null, test);
    ++currentTest;
  } else {
    onAllTestsComplete();
  }
}

function onAllTestsComplete() {
  var overallDuration = Date.now() - overallTestStartTime;
  automation.addResult("OverallTestDuration", overallDuration);
  automation.setDone();
}

// This is the only test that includes database creation and deletion in its
// results; the others just test specific operations.  To see only the
// creation/deletion without the specific operations used to build up the data
// in the object stores here, subtract off the results of
// testCreateKeysInStores.
function testCreateAndDeleteDatabase(
    numKeys, numStores, payloadLength, onTestComplete) {
  var testName = getDisplayName(arguments);
  assert(numKeys >= 0);
  assert(numStores >= 1);
  var objectStoreNames = [];
  for (var i=0; i < numStores; ++i) {
    objectStoreNames.push("store " + i);
  }
  var value = stringOfLength(payloadLength);
  function getValue() {
    return value;
  }

  automation.setStatus("Creating database.");
  var startTime = Date.now();

  createDatabase(testName, objectStoreNames, onCreated, onError);

  function onCreated(db) {
    automation.setStatus("Constructing transaction.");
    var transaction =
        getTransaction(db, objectStoreNames, "readwrite",
            function() { onValuesWritten(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, null, getValue);
  }

  function onValuesWritten(db) {
    automation.setStatus("Deleting database.");
    db.close();
    deleteDatabase(testName, onDeleted);
  }

  function onDeleted() {
    var duration = Date.now() - startTime;
    automation.addResult(testName, duration);
    automation.setStatus("Deleted database.");
    onTestComplete();
  }
}

function testCreateKeysInStores(
    numKeys, numStores, payloadLength, onTestComplete) {
  var testName = getDisplayName(arguments);
  assert(numKeys >= 0);
  assert(numStores >= 1);
  var objectStoreNames = [];
  for (var i=0; i < numStores; ++i) {
    objectStoreNames.push("store " + i);
  }
  var value = stringOfLength(payloadLength);
  function getValue() {
    return value;
  }

  automation.setStatus("Creating database.");
  createDatabase(testName, objectStoreNames, onCreated, onError);

  function onCreated(db) {
    automation.setStatus("Constructing transaction.");
    var completionFunc =
        getCompletionFunc(db, testName, Date.now(), onTestComplete);
    var transaction =
        getTransaction(db, objectStoreNames, "readwrite", completionFunc);
    putLinearValues(transaction, objectStoreNames, numKeys, null, getValue);
  }
}

function testRandomReadsAndWrites(
    numKeys, numReadsPerTransaction, numWritesPerTransaction, numTransactions,
    useIndexForReads, onTestComplete) {
  var indexName;
  if (useIndexForReads)
    indexName = "index";
  var testName = getDisplayName(arguments);
  var objectStoreNames = ["store"];
  var getKey = getSimpleKey;
  var getValue = useIndexForReads ? getIndexableValue : getSimpleValue;

  automation.setStatus("Creating database.");
  var options;
  if (useIndexForReads) {
    options = [{
      indexName: indexName,
      indexKeyPath: "id",
      indexIsUnique: false,
      indexIsMultiEntry: false,
    }];
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, null,
        function() { return "test value"; });
  }

  function onSetupComplete(db) {
    automation.setStatus("Setup complete.");
    var completionFunc =
        getCompletionFunc(db, testName, Date.now(), onTestComplete);
    var mode = "readonly";
    if (numWritesPerTransaction)
      mode = "readwrite";
    runTransactionBatch(db, numTransactions, batchFunc, objectStoreNames, mode,
        completionFunc);
  }

  function batchFunc(transaction) {
    getRandomValues(transaction, objectStoreNames, numReadsPerTransaction,
        numKeys, indexName, getKey);
    putRandomValues(transaction, objectStoreNames, numWritesPerTransaction,
        numKeys, getKey, getValue);
  }
}

function testReadCache(numTransactions, useIndexForReads, onTestComplete) {
  var numKeys = 10000;
  var numReadsPerTransaction = 50;
  var numTransactionsLeft = numTransactions;
  var indexName;
  if (useIndexForReads)
    indexName = "index";
  var testName = getDisplayName(arguments);
  var objectStoreNames = ["store"];
  var getKey = getSimpleKey;
  var getValue = useIndexForReads ? getIndexableValue : getSimpleValue;
  var keys = [];

  for (var i=0; i < numReadsPerTransaction; ++i) {
    keys.push(getKey(Math.floor(Math.random() * numKeys)));
  }

  automation.setStatus("Creating database.");
  var options;
  if (useIndexForReads) {
    options = [{
      indexName: indexName,
      indexKeyPath: "id",
      indexIsUnique: false,
      indexIsMultiEntry: false,
    }];
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, getKey,
        getValue);
  }

  var completionFunc;
  function onSetupComplete(db) {
    automation.setStatus("Setup complete.");
    completionFunc =
        getCompletionFunc(db, testName, Date.now(), onTestComplete);
    runTransactionBatch(db, numTransactions, batchFunc, objectStoreNames,
        "readonly", completionFunc);
  }

  function batchFunc(transaction) {
    getSpecificValues(transaction, objectStoreNames, indexName, keys);
  }
}

function testCreateAndDeleteIndex(numKeys, onTestComplete) {
  var testName = getDisplayName(arguments);
  var objectStoreNames = ["store"];

  automation.setStatus("Creating database.");
  createDatabase(testName, objectStoreNames, onCreated, onError);

  var startTime;
  function onCreated(db) {
    automation.setStatus("Initializing data.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onPopulated(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, null, getValue);
  }

  function getValue(i) {
    return { firstName: i + " first name", lastName: i + " last name" };
  }

  function onPopulated(db) {
    db.close();
    automation.setStatus("Building index.");
    startTime = Date.now();
    var f = function(objectStore) {
      objectStore.createIndex("index", "firstName", {unique: true});
    };
    alterObjectStores(testName, objectStoreNames, f, onIndexCreated, onError);
  }

  var indexCreationCompleteTime;
  function onIndexCreated(db) {
    db.close();
    indexCreationCompleteTime = Date.now();
    automation.addResult("testCreateIndex",
        indexCreationCompleteTime - startTime);
    var f = function(objectStore) {
      objectStore.deleteIndex("index");
    };
    automation.setStatus("Deleting index.");
    alterObjectStores(testName, objectStoreNames, f, onIndexDeleted, onError);
  }

  function onIndexDeleted(db) {
    var duration = Date.now() - indexCreationCompleteTime;
    // Ignore the cleanup time for this test.
    automation.addResult("testDeleteIndex", duration);
    automation.setStatus("Deleting database.");
    db.close();
    deleteDatabase(testName, onDeleted);
  }

  function onDeleted() {
    automation.setStatus("Deleted database.");
    onTestComplete();
  }
}

function testCursorReadsAndRandomWrites(
    readKeysOnly, useIndexForReads, writeAlso, sameStoreForWrites,
    onTestComplete) {
  // There's no key cursor unless you're reading from an index.
  assert(useIndexForReads || !readKeysOnly);
  // If we're writing to another store, having an index would constrain our
  // writes, as we create both object stores with the same configurations.
  // We could do that if needed, but it's simpler not to.
  assert(!useIndexForReads || !writeAlso);
  var numKeys = 10000;
  var numReadsPerTransaction = 1000;
  var testName = getDisplayName(arguments);
  var objectStoreNames = ["input store"];
  var outputStoreName;
  if (writeAlso) {
    if (sameStoreForWrites) {
      outputStoreName = objectStoreNames[0];
    } else {
      outputStoreName = "output store";
      objectStoreNames.push(outputStoreName);
    }
  }
  var getKeyForRead = getSimpleKey;
  var indexName;
  if (useIndexForReads) {
    indexName = "index";
    getKeyForRead = function(i) {
      // This depends on the implementations of getValuesFromCursor and
      // getObjectValue.  We reverse the order of the iteration here so that
      // setting up bounds from k to k+n with n>0 works.  Without this reversal,
      // the upper bound is below the lower bound.
      return getBackwardIndexKey(numKeys - i);
    };
  }

  automation.setStatus("Creating database.");
  var options;
  if (useIndexForReads) {
    options = [{
      indexName: indexName,
      indexKeyPath: "lastName", // depends on getBackwardIndexKey()
      indexIsUnique: true,
      indexIsMultiEntry: false,
    }];
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, getSimpleKey,
        getObjectValue);
  }
  function onSetupComplete(db) {
    automation.setStatus("Setup complete.");
    var completionFunc =
        getCompletionFunc(db, testName, Date.now(), onTestComplete);
    var mode = "readonly";
    if (writeAlso)
      mode = "readwrite";
    var transaction =
        getTransaction(db, objectStoreNames, mode, completionFunc);

    getValuesFromCursor(
        transaction, objectStoreNames[0], numReadsPerTransaction, numKeys,
        indexName, getKeyForRead, readKeysOnly, outputStoreName);
  }
}

function testSporadicWrites(
    numOperationsPerTransaction, numIndices, alternateWithReads,
    onTestComplete) {
  var numKeys = 1000;
  // With 30 transactions, spaced 50ms apart, we'll need at least 1.5s.
  var numTransactions = 30;
  if (alternateWithReads)
    numTransactions *= 2;
  var delayBetweenBatches = 50;
  var indexName;
  var testName = getDisplayName(arguments);
  var numTransactionsLeft = numTransactions;
  var objectStoreNames = ["store"];
  var numTransactionsRunning = 0;

  var getValue = getSimpleValue;
  if (numIndices)
    getValue = function (i) { return getNFieldObjectValue(i, numIndices); };

  automation.setStatus("Creating database.");
  var options = [];
  for (var i=0; i < numIndices; ++i) {
    var o = {};
    o.indexName = "index " + i;
    o.indexKeyPath = getNFieldName(i);
    o.indexIsUnique = false;
    o.indexIsMultiEntry = false;
    options.push(o);
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, getSimpleKey,
        getValue);
  }
  var completionFunc;
  function onSetupComplete(db) {
    automation.setStatus("Setup complete.");
    completionFunc =
        getCompletionFunc(db, testName, Date.now(), onTestComplete);
    runOneBatch(db);
  }

  function runOneBatch(db) {
    assert(numTransactionsLeft);
    if (--numTransactionsLeft) {
      setTimeout(function () { runOneBatch(db); }, delayBetweenBatches);
    }

    var mode, transaction;
    if (alternateWithReads) {
      ++numTransactionsRunning;
      transaction =
          getTransaction(db, objectStoreNames, "readonly", batchComplete);
      getRandomValues(transaction, objectStoreNames,
          numOperationsPerTransaction, numKeys);
    }
    ++numTransactionsRunning;
    transaction =
        getTransaction(db, objectStoreNames, "readwrite", batchComplete);
    putRandomValues(transaction, objectStoreNames, numOperationsPerTransaction,
        numKeys);

    function batchComplete() {
      assert(numTransactionsRunning);
      if (!--numTransactionsRunning && !numTransactionsLeft)
        completionFunc();
    }
  }
}

function testWalkingMultipleCursors(numCursors, onTestComplete) {
  var numKeys = 1000;
  var numHitsPerKey = 10;
  var testName = getDisplayName(arguments);
  var objectStoreNames = ["input store"];
  var indexName = "index name";
  var getKey = getSimpleKey;
  var getValue = getIndexableValue;

  automation.setStatus("Creating database.");
  var options = [{
    indexName: indexName,
    indexKeyPath: "id",
    indexIsUnique: false,
    indexIsMultiEntry: false,
  }];
  createDatabase(testName, objectStoreNames, onCreated, onError, options);

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    // This loop adds the same value numHitsPerKey times for each key.
    for (var i = 0; i < numHitsPerKey; ++i) {
      putLinearValues(transaction, objectStoreNames, numKeys, getKeyFunc(i),
          getValue);
    }
  }
  // While the value is the same each time through the putLinearValues loop, we
  // want the key to keep increaasing for each copy.
  function getKeyFunc(k) {
    return function(i) {
      return getKey(k * numKeys + i);
    };
  }
  var completionFunc;
  function onSetupComplete(db) {
    automation.setStatus("Setup complete.");
    completionFunc =
        getCompletionFunc(db, testName, Date.now(), onTestComplete);
    var transaction =
        getTransaction(db, objectStoreNames, "readonly", verifyComplete);

    walkSeveralCursors(transaction, numKeys);
  }
  var responseCounts = [];
  var cursorsRunning = numCursors;
  function walkSeveralCursors(transaction, numKeys) {
    var source = transaction.objectStore(objectStoreNames[0]).index(indexName);
    var requests = [];
    var continueCursorIndex = 0;
    for (var i = 0; i < numCursors; ++i) {
      var rand = Math.floor(Math.random() * numKeys);
      // Since we have numHitsPerKey copies of each value in the database,
      // IDBKeyRange.only will return numHitsPerKey results, each referring to a
      // different key with the matching value.
      var request = source.openCursor(IDBKeyRange.only(getSimpleValue(rand)));
      responseCounts.push(0);
      request.onerror = onError;
      request.onsuccess = function(event) {
        assert(cursorsRunning);
        var request = event.target;
        if (!("requestIndex" in request)) {
          assert(requests.length < numCursors);
          request.requestIndex = requests.length;
          requests.push(request);
        }
        var cursor = event.target.result;
        if (cursor) {
          assert(responseCounts[request.requestIndex] < numHitsPerKey);
          ++responseCounts[request.requestIndex];
        } else {
          assert(responseCounts[request.requestIndex] == numHitsPerKey);
          --cursorsRunning;
        }
        if (cursorsRunning) {
          if (requests.length == numCursors) {
            requests[continueCursorIndex++].result.continue();
            continueCursorIndex %= numCursors;
          }
        }
      };
    }
  }
  function verifyComplete() {
    assert(!cursorsRunning);
    completionFunc();
  }
}

