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

  chrome.test.runTests([
    function downloadFilename() {
      chrome.experimental.downloads.download(
        {"url": getURL("pass"), "filename": "pass"},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
      // TODO(benjhayden): Test the filename using onChanged.
    },
    function downloadSubDirectoryFilename() {
      chrome.experimental.downloads.download(
        {"url": getURL("pass"), "filename": "foo/pass"},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
      // TODO(benjhayden): Test the filename using onChanged.
    },
    function downloadInvalidFilename() {
      chrome.experimental.downloads.download(
        {"url": getURL("pass"), "filename": "../../../../../etc/passwd"},
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
                   {"url": getURL("pass"), "saveAs": "GOAT"},
                   ("Invalid value for argument 1. Property 'saveAs': " +
                    "Expected 'boolean' but got 'string'."));
    },
    function downloadInvalidHeadersOption() {
      assertThrows(chrome.experimental.downloads.download,
                   {"url": getURL("pass"), "headers": "GOAT"},
                   ("Invalid value for argument 1. Property 'headers': " +
                    "Expected 'array' but got 'string'."));
    },
    function downloadInvalidURL() {
      chrome.experimental.downloads.download(
        {"url": "foo bar"},
        chrome.test.callbackFail("Invalid URL"));
    },
    function downloadInvalidMethod() {
      assertThrows(chrome.experimental.downloads.download,
                   {"url": getURL("pass"), "method": "GOAT"},
                   ("Invalid value for argument 1. Property 'method': " +
                    "Value must be one of: [GET, POST]."));
    },
    function downloadSimple() {
      chrome.experimental.downloads.download(
        {"url": getURL("pass")},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
      }));
    },
    function downloadHeader() {
      chrome.experimental.downloads.download(
        {"url": getURL("pass"),
         "headers": [{"name": "Foo", "value": "bar"}]},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
        }));
    },
    function downloadInterrupted() {
      chrome.experimental.downloads.download(
        {"url": getURL("compressedfiles/Picture_1.doc?L")},
        chrome.test.callbackPass(function(id) {
          chrome.test.assertEq(getNextId(), id);
          // TODO(benjhayden): Test that this id is eventually interrupted using
          // onChanged.
      }));
    },
    function downloadInvalidHeader() {
      chrome.experimental.downloads.download(
        {"url": getURL("pass"),
         "headers": [{"name": "Cookie", "value": "fake"}]},
        chrome.test.callbackFail("I'm afraid I can't do that."));
      // TODO(benjhayden): Give a better error message.
    }
  ]);
});
