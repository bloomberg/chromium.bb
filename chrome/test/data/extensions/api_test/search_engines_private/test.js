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
    chrome.searchEnginesPrivate.addOtherSearchEngine(
        'name1', 'search1.com', 'http://search1.com');
    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      for (var i = 0; i < engines.length; i++) {
        if (engines[i].name == 'name1') {
          chrome.test.assertTrue(!engines[i].isSelected);
          chrome.searchEnginesPrivate.setSelectedSearchEngine(engines[i].guid);
        }
      }

      chrome.searchEnginesPrivate.getSearchEngines(function(newEngines) {
        for (var i = 0; i < newEngines.length; i++) {
          if (newEngines[i].name == 'name1') {
            chrome.test.assertTrue(newEngines[i].isSelected);
            chrome.test.succeed();
            return;
          }
        }
        chrome.test.fail();
      });
    });
  },

  function onSearchEnginesChanged() {
    chrome.searchEnginesPrivate.addOtherSearchEngine(
        'name1', 'search1.com', 'http://search1.com/%s');
    chrome.searchEnginesPrivate.onSearchEnginesChanged.addListener(
        function(engines) {
          for (var i = 0; i < engines.length; i++) {
            if (engines[i].name == 'name1') {
              chrome.test.assertTrue(engines[engines.length - 1].isSelected);
              chrome.test.succeed();
              return;
            }
          }
          chrome.test.fail();
        });

    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      // Setting an 'other' search engine as default should cause the
      // onSearchEnginesChanged event to fire.
      chrome.searchEnginesPrivate.setSelectedSearchEngine(
          engines[engines.length - 1].guid);
    });
  },

  function addNewSearchEngine() {
    var testName = 'name';
    var testKeyword = 'search.com';
    var testUrl = 'http://search.com';
    chrome.searchEnginesPrivate.addOtherSearchEngine(
        testName, testKeyword, testUrl);
    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      for (var i = 0; i < engines.length; i++) {
        if (engines[i].name == testName) {
          chrome.test.assertEq(testKeyword, engines[i].keyword);
          chrome.test.assertEq(testUrl, engines[i].url);
          chrome.test.succeed();
          return;
        }
      }
      chrome.test.fail();
    });
  },

  function updateSearchEngine() {
    chrome.searchEnginesPrivate.addOtherSearchEngine(
        'name1', 'search1.com', 'http://search1.com');
    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      chrome.searchEnginesPrivate.updateSearchEngine(
          engines[0].guid, 'name2', 'search2.com', 'http://search2.com');
      chrome.searchEnginesPrivate.getSearchEngines(function(newEngines) {
        chrome.test.assertEq('name2', newEngines[0].name);
        chrome.test.assertEq('search2.com', newEngines[0].keyword);
        chrome.test.assertEq('http://search2.com', newEngines[0].url);
        chrome.test.succeed();
      });
    });
  },

  function removeSearchEngine() {
    chrome.searchEnginesPrivate.addOtherSearchEngine(
        'name1', 'search1.com', 'http://search1.com');
    chrome.searchEnginesPrivate.addOtherSearchEngine(
        'name2', 'search2.com', 'http://search2.com');
    chrome.searchEnginesPrivate.getSearchEngines(function(engines) {
      var engine1Guid = engines[1].guid;
      chrome.searchEnginesPrivate.removeSearchEngine(engine1Guid);

      chrome.searchEnginesPrivate.getSearchEngines(function(newEngines) {
        for (var i = 0; i < newEngines.length; i++) {
          chrome.test.assertFalse(newEngines[i].guid == engine1Guid);
        }
        chrome.test.succeed();
      });
    });
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));

