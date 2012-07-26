// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var overallTestStartTime = Date.now();

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

  function onCreated(db) {
    automation.setStatus("Constructing transaction.");
    var completionFunc =
        getCompletionFunc(testName, Date.now(), onTestComplete);
    var transaction =
        getTransaction(db, objectStoreNames, "readwrite", completionFunc);
    putLinearValues(transaction, objectStoreNames, numKeys, null, getValue);
  }
  automation.setStatus("Creating database.");
  createDatabase(testName, objectStoreNames, onCreated, onError);
}

function testRandomReadsAndWrites(
    numKeys, numReadsPerTransaction, numWritesPerTransaction, numTransactions,
    useIndexForReads, onTestComplete) {
  var indexName;
  if (useIndexForReads)
    indexName = "index";
  var testName = getDisplayName(arguments);
  var numTransactionsLeft = numTransactions;
  var objectStoreNames = ["store"];
  var numTransactionsRunning;

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, null,
        function () { return "test value"; });
  }
  var completionFunc;
  function onSetupComplete(db) {
    automation.setStatus("Setup complete.");
    runOneBatch(db);
    completionFunc = getCompletionFunc(testName, Date.now(), onTestComplete);
  }

  function runOneBatch(db) {
    if (numTransactionsLeft <= 0) {
      return;
    }
    --numTransactionsLeft;
    ++numTransactionsRunning;
    var mode = "readonly";
    if (numWritesPerTransaction)
      mode = "readwrite";
    var transaction = getTransaction(db, objectStoreNames, mode,
        function() {
          assert(!--numTransactionsRunning);
          if (numTransactionsLeft <= 0) {
            completionFunc();
          } else {
            runOneBatch(db);
          }
        });

    getRandomValues(transaction, objectStoreNames, numReadsPerTransaction,
        numKeys, indexName);
    putRandomValues(transaction, objectStoreNames, numWritesPerTransaction,
        numKeys);
  }

  automation.setStatus("Creating database.");
  var options = {};
  if (useIndexForReads) {
    options.indexName = indexName;
    options.indexKeyPath = "";
    options.indexIsUnique = false;
    options.indexIsMultiEntry = false;
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);
}

function testCreateAndDeleteIndex(numKeys, onTestComplete) {
  var testName = getDisplayName(arguments);
  var objectStoreNames = ["store"];
  function getValue(i) {
    return { firstName: i + " first name", lastName: i + " last name" };
  }

  var startTime;
  function onCreated(db) {
    automation.setStatus("Initializing data.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onPopulated(db); });
    putLinearValues(transaction, objectStoreNames, numKeys, null, getValue);
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

  var completionFunc;
  function onIndexCreated(db) {
    db.close();
    var indexCreationCompleteTime = Date.now();
    automation.addResult("testCreateIndex",
        indexCreationCompleteTime - startTime);
    completionFunc = getCompletionFunc("testDeleteIndex",
        indexCreationCompleteTime, onTestComplete);
    var f = function(objectStore) {
      objectStore.deleteIndex("index");
    };
    automation.setStatus("Deleting index.");
    alterObjectStores(testName, objectStoreNames, f, completionFunc, onError);
  }

  automation.setStatus("Creating database.");
  createDatabase(testName, objectStoreNames, onCreated, onError);
}

var kUseIndex = true;
var kDontUseIndex = false;
var tests = [
  [testCreateKeysInStores, 1,     1,    1],
  [testCreateKeysInStores, 100,   1,    1],
  [testCreateKeysInStores, 1,     100,  1],
  [testCreateKeysInStores, 100,   1,    10000],
  [testRandomReadsAndWrites, 1000,  5,    0,  50, kDontUseIndex],
  [testRandomReadsAndWrites, 1000,  50,   0,  5,  kDontUseIndex],
  [testRandomReadsAndWrites, 5000,  50,   0,  5,  kDontUseIndex],
  [testRandomReadsAndWrites, 1000,  5,    0,  50, kUseIndex],
  [testRandomReadsAndWrites, 1000,  50,   0,  5,  kUseIndex],
  [testRandomReadsAndWrites, 5000,  50,   0,  5,  kUseIndex],
  [testRandomReadsAndWrites, 1000,  5,    5,  50, kDontUseIndex],
  [testRandomReadsAndWrites, 1000,  5,    5,  50, kUseIndex],
  [testCreateAndDeleteIndex, 1000]
];

var currentTest = 0;

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

function test() {
  runNextTest();
}
