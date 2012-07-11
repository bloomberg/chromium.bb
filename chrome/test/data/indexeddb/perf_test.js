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
  var start;

  function onCreated(db) {
    automation.setStatus("Constructing transaction.");
    start = Date.now(); // Ignore the setup time for this test.
    var transaction = getTransaction(db, objectStoreNames, "readwrite",
        function() { onTransactionComplete(db); });
    for (var i in objectStoreNames) {
      var os = transaction.objectStore(objectStoreNames[i]);
      assert(os);
      for (var j = 0; j < numKeys; ++j) {
        os.put("key " + j, value);
      }
    }
  }
  function onTransactionComplete(db) {
    var duration = Date.now() - start;
    // Ignore the cleanup time for this test.
    automation.addResult(testName, duration);
    automation.setStatus("Deleting.");
    deleteDatabase(db, onDeleted);
  }
  function onDeleted() {
    automation.setStatus("Deleted.");
    onTestComplete();
  }
  automation.setStatus("Creating.");
  createDatabase(testName, objectStoreNames, onCreated, onError);
}

var tests = [
  [testCreateKeysInStores, 1,    1,    1],
  [testCreateKeysInStores, 100,  1,    1],
  [testCreateKeysInStores, 1,    100,  1],
  [testCreateKeysInStores, 100,  1,    100000]
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
