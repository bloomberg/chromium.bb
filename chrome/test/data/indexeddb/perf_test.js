// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var overallTestStartTime = Date.now();

function testCreateKeysInStores(
    numKeys, numStores, payloadLength, onTestComplete) {
  var testName = "testCreateKeysInStores_" + numKeys + "_" + numStores + "_" +
      payloadLength;
  assert(numKeys >= 0);
  assert(numStores >= 1);
  var objectStoreNames = [];
  for (var i=0; i < numStores; ++i) {
    objectStoreNames.push("store " + i);
  }
  var value = stringOfLength(payloadLength);

  function onCreated(db) {
    automation.setStatus("Constructing transaction.");
    var completionFunc =
        getCompletionFunc(testName, Date.now(), onTestComplete);
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { completionFunc(); });
    for (var i in objectStoreNames) {
      var os = transaction.objectStore(objectStoreNames[i]);
      assert(os);
      for (var j = 0; j < numKeys; ++j) {
        os.put(value, "key " + j);
      }
    }
  }
  automation.setStatus("Creating database.");
  createDatabase(testName, objectStoreNames, onCreated, onError);
}

function testRandomReads(numKeys, numReadsPerTransaction, numTransactions,
    useIndex, onTestComplete) {
  var indexText = "_bare";
  var indexName;
  if (useIndex) {
    indexText = "_index";
    indexName = "index";
  }
  var testName = "testRandomReads_" + numKeys + "_" + numReadsPerTransaction +
      "_" + numTransactions + indexText;
  var numTransactionsLeft = numTransactions;
  var storeName = "store";
  var objectStoreNames = [storeName];
  var numTransactionsRunning;

  function getKey(i) {
    return "key " + i;
  }

  function getValue(i) {
    return "value " + i;
  }

  function onCreated(db) {
    automation.setStatus("Setting up test database.");
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onSetupComplete(db); });
    var os = transaction.objectStore(storeName);
    assert(os);
    for (var j = 0; j < numKeys; ++j) {
      os.put(getValue(i), getKey(j));
    }
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
    var valuesToRead = numReadsPerTransaction;
    var transaction = getTransaction(db, objectStoreNames, "readonly",
        function() {
          assert(!--numTransactionsRunning);
          assert(!valuesToRead);
          if (numTransactionsLeft <= 0) {
            completionFunc();
          } else {
            runOneBatch(db);
          }
        });

    var queryObject = transaction.objectStore(storeName);
    if (useIndex) {
      queryObject = queryObject.index(indexName);
    }
    assert(queryObject);
    for (var i = 0; i < numReadsPerTransaction; ++i) {
      var rand = Math.floor(Math.random() * numKeys);
      var request = queryObject.get(getKey(rand));
      request.onerror = onError;
      request.onsuccess = function () {
        assert(valuesToRead--);
      }
    }
  }

  automation.setStatus("Creating database.");
  var options = {};
  if (useIndex) {
    options.indexName = indexName;
    options.indexKeyPath = "";
    options.indexIsUnique = true;
    options.indexIsMultiEntry = false;
  }
  createDatabase(testName, objectStoreNames, onCreated, onError, options);
}

var kUseIndex = true;
var kDontUseIndex = false;

var tests = [
  [testCreateKeysInStores, 1,     1,    1],
  [testCreateKeysInStores, 100,   1,    1],
  [testCreateKeysInStores, 1,     100,  1],
  [testCreateKeysInStores, 100,   1,    10000],
  [testRandomReads,        1000,  5,    50, kDontUseIndex],
  [testRandomReads,        1000,  50,   5,  kDontUseIndex],
  [testRandomReads,        5000,  50,   5,  kDontUseIndex],
  [testRandomReads,        1000,  5,    50, kUseIndex],
  [testRandomReads,        1000,  50,   5,  kUseIndex],
  [testRandomReads,        5000,  50,   5,  kUseIndex]
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
