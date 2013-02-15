// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryApiTest.DeleteProhibited

var PROHIBITED_ERR = "Browsing history is not allowed to be deleted.";

function deleteProhibitedTestVerification() {
  removeItemRemovedListener();
  chrome.test.fail("Delete was prohibited, but succeeded anyway.");
}

// Order history results by ID for reliable array comparison.
function sortResults(a, b) {
  return (a.id - b.id);
}

// Run the provided function (which must be curried with arguments if needed)
// and verify that it both returns an error and does not delete any items from
// history.
function verifyNoDeletion(testFunction) {
  setItemRemovedListener(deleteProhibitedTestVerification);

  var query = { 'text': '' };
  chrome.history.addUrl({ url: GOOGLE_URL }, pass(function() {
    chrome.history.addUrl({ url: PICASA_URL }, pass(function() {
      chrome.history.search(query, pass(function(resultsBefore) {
        testFunction(fail(PROHIBITED_ERR, function() {
          chrome.history.search(query, pass(function (resultsAfter) {
            assertEq(resultsBefore.sort(sortResults),
                     resultsAfter.sort(sortResults));
            removeItemRemovedListener();
          }));
        }));
      }));
    }));
  }));
}

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  function deleteUrl() {
    verifyNoDeletion(function(callback) {
      chrome.history.deleteUrl({ 'url': GOOGLE_URL }, callback);
    });
  },

  function deleteRange() {
    var now = new Date();
    verifyNoDeletion(function(callback) {
      chrome.history.deleteRange(
          { 'startTime': 0, 'endTime': now.getTime() + 1000.0 }, callback);
    });
  },

  function deleteAll() {
    verifyNoDeletion(chrome.history.deleteAll);
  }
]);
