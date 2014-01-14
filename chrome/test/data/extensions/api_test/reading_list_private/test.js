// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reading List Private API test for Chrome.
// browser_tests.exe --gtest_filter=ReadingListPrivateApiTest.*

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;
var readingList = chrome.readingListPrivate;

var tests = [
  function addAndRemoveEntry() {
    readingList.addEntry({url: "http://www.google.com"},
        pass(function(newEntry) {
      readingList.getEntries(pass(function(entries) {
        assertEq([newEntry], entries);
        readingList.removeEntry(newEntry.id, pass(function(removedEntry) {
          assertEq(removedEntry, newEntry),
          readingList.getEntries(pass(function(entries) {
            assertEq([], entries);
          }));
        }));
      }));
    }));
  },

  function addInvalidUrl() {
    readingList.addEntry({url: "www.google.com"},
        fail("Invalid url specified."));
  },

  function removeNonExistentEntry() {
    readingList.removeEntry("lkj", pass(function(removedEntry) {
      assertEq(undefined, removedEntry);
    }));
  }
];

chrome.test.runTests(tests);
