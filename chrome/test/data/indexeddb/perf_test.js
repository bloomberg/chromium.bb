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
  [testSporadicWrites, 5, 0],
// Make large batches of random writes into a store, triggered by periodic
// setTimeout calls.
  [testSporadicWrites, 500, 0],
// Make batches of random writes into a store with many indices, triggered by
// periodic setTimeout calls.
  [testSporadicWrites, 5, 10],
// Make large batches of random writes into a store with many indices, triggered
// by periodic setTimeout calls.
  [testSporadicWrites, 500, 10],
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
  [testCreateAndDeleteIndex, 5000]
];

var currentTest = 0;

function test() {
  runNextTest();
}

function runNextTest() {
  if (currentTest < tests.length) {
    var test = tests[currentTest++].slice();
    var f = test.shift();
    test.push(runNextTest);
    f.apply(null, test);
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

  automation.setStatus("Creating database.");
  var options;
  if (useIndexForReads) {
    options = [{
      indexName: indexName,
      indexKeyPath: "",
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
        numKeys, indexName);
    putRandomValues(transaction, objectStoreNames, numWritesPerTransaction,
        numKeys);
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
  var keys = [];

  for (var i=0; i < numReadsPerTransaction; ++i) {
    keys.push(getSimpleKey(Math.floor(Math.random() * numKeys)));
  }

  automation.setStatus("Creating database.");
  var options;
  if (useIndexForReads) {
    options = [{
      indexName: indexName,
      indexKeyPath: "",
      indexIsUnique: false,
      indexIsMultiEntry: false,
    }];
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, getSimpleKey,
        function () { return "test value"; });
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
    }
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
    numWritesPerTransaction, numIndices, onTestComplete) {
  var numKeys = 1000;
  // With 30 transactions, spaced 50ms apart, we'll need at least 1.5s.
  var numTransactions = 30;
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
    ++numTransactionsRunning;

    function batchComplete() {
      assert(numTransactionsRunning);
      if (!--numTransactionsRunning && !numTransactionsLeft)
        completionFunc();
    }

    var mode = "readonly";
    if (numWritesPerTransaction)
      mode = "readwrite";

    var transaction = getTransaction(db, objectStoreNames, mode, batchComplete);
    putRandomValues(transaction, objectStoreNames, numWritesPerTransaction,
        numKeys);
  }
}
