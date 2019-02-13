// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testSetStorage = function(storageArea, key, value) {
  var options = {};
  options[key] = value;
  try {
    storageArea.set(options, function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var testGetStorage = function(storageArea, key, expectedValue) {
  try {
    storageArea.get([key], function(result) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq(expectedValue, result[key]);
      chrome.test.succeed();
    });
  }
  catch (e) {
    chrome.test.fail(e);
  }
};

var localKey = '_local_key';
var localValue = 'this is a local value';
var syncKey = '_sync_key';
var syncValue = 'this is a sync value';

chrome.test.runTests([
  function testLocalSet() {
    testSetStorage(chrome.storage.local, localKey, localValue);
  },
  function testLocalGet() {
    testGetStorage(chrome.storage.local, localKey, localValue);
  },
  function testSyncSet() {
    testSetStorage(chrome.storage.sync, syncKey, syncValue);
  },
  function testSyncGet() {
    testGetStorage(chrome.storage.sync, syncKey, syncValue);
  },
]);
