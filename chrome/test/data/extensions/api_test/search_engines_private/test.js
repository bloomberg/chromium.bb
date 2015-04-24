// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.

function callbackResult(result) {
  if (chrome.runtime.lastError)
    chrome.test.fail(chrome.runtime.lastError.message);
  else if (result == false)
    chrome.test.fail('Failed: ' + result);
}

var availableTests = [
  function setSelectedSearchEngine() {
    chrome.searchEnginesPrivate.getDefaultSearchEngines(function(engines) {
      chrome.searchEnginesPrivate.setSelectedSearchEngine(
          engines[engines.length - 1].guid);
      chrome.searchEnginesPrivate.getDefaultSearchEngines(function(newEngines) {
        chrome.test.assertTrue(newEngines[newEngines.length - 1].isSelected);
        chrome.test.succeed();
      });
    });
  },

  function onDefaultSearchEnginesChanged() {
    chrome.searchEnginesPrivate.onDefaultSearchEnginesChanged.addListener(
        function(engines) {
          chrome.test.assertTrue(engines[1].isSelected,
              'Engine 1 should be selected');
          chrome.test.succeed();
        });
    chrome.searchEnginesPrivate.getDefaultSearchEngines(function(engines) {
      chrome.searchEnginesPrivate.setSelectedSearchEngine(engines[1].guid);
    });
  }
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

