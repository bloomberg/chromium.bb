// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var checkUnordererArrayEquality = function(expected, actual) {
  chrome.test.assertEq(expected.length, actual.length);

  var expectedSet = new Set(expected);
  for (var i = 0; i < actual.length; ++i) {
    chrome.test.assertTrue(
        expectedSet.has(actual[i]),
        JSON.stringify(actual[i]) + ' is not in the expected set');
  }
};

chrome.test.runTests([
  function addWhitelistedPages() {
    var toAdd = ['https://www.google.com/', 'https://www.yahoo.com/'];
    chrome.declarativeNetRequest.addWhitelistedPages(
        toAdd, callbackPass(function() {}));
  },

  function verifyAddWhitelistedPages() {
    chrome.declarativeNetRequest.getWhitelistedPages(
        callbackPass(function(patterns) {
          checkUnordererArrayEquality(
              ['https://www.google.com/', 'https://www.yahoo.com/'], patterns);
        }));
  },

  function removeWhitelistedPages() {
    // It's ok for |toRemove| to specify a pattern which is not currently
    // whitelisted.
    var toRemove = ['https://www.google.com/', 'https://www.reddit.com/'];
    chrome.declarativeNetRequest.removeWhitelistedPages(
        toRemove, callbackPass(function() {}));
  },

  function verifyRemoveWhitelistedPages() {
    chrome.declarativeNetRequest.getWhitelistedPages(
        callbackPass(function(patterns) {
          checkUnordererArrayEquality(['https://www.yahoo.com/'], patterns);
        }));
  },

  function verifyErrorOnAddingInvalidMatchPattern() {
    // The second match pattern here is invalid. The current set of
    // whitelisted pages won't change since no addition is performed in case
    // any of the match patterns are invalid.
    var toAdd = ['https://www.reddit.com/', 'https://google*.com/'];
    var expectedError = 'Invalid url pattern \'https://google*.com/\'';
    chrome.declarativeNetRequest.addWhitelistedPages(
        toAdd, callbackFail(expectedError));
  },

  function verifyErrorOnRemovingInvalidMatchPattern() {
    // The second match pattern here is invalid since it has no path
    // component. current set of whitelisted pages won't change since no
    // removal is performed in case any of the match patterns are invalid.
    var toRemove = ['https://yahoo.com/', 'https://www.reddit.com'];
    var expectedError = 'Invalid url pattern \'https://www.reddit.com\'';
    chrome.declarativeNetRequest.removeWhitelistedPages(
        toRemove, callbackFail(expectedError));
  },

  function verifyExpectedPatternSetDidNotChange() {
    chrome.declarativeNetRequest.getWhitelistedPages(
        callbackPass(function(patterns) {
          checkUnordererArrayEquality(['https://www.yahoo.com/'], patterns);
        }));
  }
]);
