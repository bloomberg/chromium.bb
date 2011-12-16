// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// downloads api test
// browser_tests.exe --gtest_filter=DownloadsApiTest.Downloads

chrome.test.getConfig(function(testConfig) {
  function getURL(path) {
    return "http://localhost:" + testConfig.testServer.port + "/" + path;
  }

  var nextId = 0;
  function getNextId() {
    return nextId++;
  }

  // There can be only one assertThrows per test function.
  function assertThrows(func, arg, expected_message) {
    try {
      func(arg, function() {
        // Don't use chrome.test.callbackFail because it requires the
        // callback to be called.
        chrome.test.fail("Failed to throw exception (" +
                         expected_message + ")");
      });
    } catch (exception) {
      chrome.test.assertEq(expected_message, exception.message);
      chrome.test.succeed();
    }
  }

  // The "/slow" handler waits a specified amount of time before returning a
  // safe file. Specify zero seconds to return quickly.
  var SAFE_FAST_URL = getURL("slow?0");

  var ERROR_GENERIC = chrome.experimental.downloads.ERROR_GENERIC;
  var ERROR_INVALID_URL = chrome.experimental.downloads.ERROR_INVALID_URL;
  var ERROR_INVALID_OPERATION =
    chrome.experimental.downloads.ERROR_INVALID_OPERATION;

  chrome.test.runTests([
    // TODO(benjhayden): Test onErased using remove().
    function downloadFilename() {
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL, "filename": "foo"},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
      // TODO(benjhayden): Test the filename using onChanged.
    },
    function downloadOnCreated() {
      chrome.test.listenOnce(chrome.experimental.downloads.onCreated,
        chrome.test.callbackPass(function(item) {
      }));
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL},
        function(id) {
          chrome.test.assertEq(getNextId(), id);
      });
    },
    function downloadSubDirectoryFilename() {
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL, "filename": "foo/slow"},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
      // TODO(benjhayden): Test the filename using onChanged.
    },
    function downloadInvalidFilename() {
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL, "filename": "../../../../../etc/passwd"},
        chrome.test.callbackFail("I'm afraid I can't do that."));
      // TODO(benjhayden): Give a better error message.
    },
    function downloadEmpty() {
      assertThrows(chrome.experimental.downloads.download, {},
                   ("Invalid value for argument 1. Property 'url': " +
                    "Property is required."));
    },
    function downloadInvalidSaveAs() {
      assertThrows(chrome.experimental.downloads.download,
                   {"url": SAFE_FAST_URL, "saveAs": "GOAT"},
                   ("Invalid value for argument 1. Property 'saveAs': " +
                    "Expected 'boolean' but got 'string'."));
    },
    function downloadInvalidHeadersOption() {
      assertThrows(chrome.experimental.downloads.download,
                   {"url": SAFE_FAST_URL, "headers": "GOAT"},
                   ("Invalid value for argument 1. Property 'headers': " +
                    "Expected 'array' but got 'string'."));
    },
    function downloadInvalidURL() {
      chrome.experimental.downloads.download(
        {"url": "foo bar"},
        chrome.test.callbackFail(ERROR_INVALID_URL));
    },
    function downloadInvalidMethod() {
      assertThrows(chrome.experimental.downloads.download,
                   {"url": SAFE_FAST_URL, "method": "GOAT"},
                   ("Invalid value for argument 1. Property 'method': " +
                    "Value must be one of: [GET, POST]."));
    },
    function downloadSimple() {
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
    },
    function downloadHeader() {
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL,
         "headers": [{"name": "Foo", "value": "bar"}]},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
        }));
    },
    function downloadInterrupted() {
      // TODO(benjhayden): Find a suitable URL and test that this id is
      // eventually interrupted using onChanged.
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
    },
    function downloadInvalidHeader() {
      chrome.experimental.downloads.download(
        {"url": SAFE_FAST_URL,
         "headers": [{"name": "Cookie", "value": "fake"}]},
        chrome.test.callbackFail(ERROR_GENERIC));
      // TODO(benjhayden): Give a better error message.
    },
    function downloadPauseInvalidId() {
      chrome.experimental.downloads.pause(-42,
        chrome.test.callbackFail(ERROR_INVALID_OPERATION));
    },
    function downloadPauseInvalidType() {
      assertThrows(chrome.experimental.downloads.pause,
                   "foo",
                   ("Invalid value for argument 1. Expected 'integer' " +
                    "but got 'string'."));
    },
    function downloadResumeInvalidId() {
      chrome.experimental.downloads.resume(-42,
        chrome.test.callbackFail(ERROR_INVALID_OPERATION));
    },
    function downloadResumeInvalidType() {
      assertThrows(chrome.experimental.downloads.resume,
                   "foo",
                   ("Invalid value for argument 1. Expected 'integer' " +
                    "but got 'string'."));
    },
    function downloadCancelInvalidId() {
      // Canceling a non-existent download is not considered an error.
      chrome.experimental.downloads.cancel(-42,
        chrome.test.callbackPass(function() {}));
    },
    function downloadCancelInvalidType() {
      assertThrows(chrome.experimental.downloads.cancel,
                   "foo",
                   ("Invalid value for argument 1. Expected 'integer' " +
                    "but got 'string'."));
    }
  ]);
});
