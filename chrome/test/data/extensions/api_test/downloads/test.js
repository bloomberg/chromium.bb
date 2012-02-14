// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// downloads api test
// browser_tests.exe --gtest_filter=DownloadsApiTest.Downloads

var downloads = chrome.experimental.downloads;

chrome.test.getConfig(function(testConfig) {
  function getURL(path) {
    return 'http://localhost:' + testConfig.testServer.port + '/' + path;
  }

  var nextId = 0;
  function getNextId() {
    return nextId++;
  }

  // There can be only one assertThrows per test function.  This should be
  // called as follows:
  //
  //   assertThrows(exceptionMessage, function, [arg1, [arg2, ... ]])
  //
  // ... where |exceptionMessage| is the expected exception message.
  // |arg1|, |arg2|, etc.. are arguments to |function|.
  function assertThrows(exceptionMessage, func) {
    var args = Array.prototype.slice.call(arguments, 2);
    try {
      args.push(function() {
        // Don't use chrome.test.callbackFail because it requires the
        // callback to be called.
        chrome.test.fail('Failed to throw exception (' +
                         exceptionMessage + ')');
      });
      func.apply(null, args);
    } catch (exception) {
      chrome.test.assertEq(exceptionMessage, exception.message);
      chrome.test.succeed();
    }
  }

  // The "/slow" handler waits a specified amount of time before returning a
  // safe file. Specify zero seconds to return quickly.
  var SAFE_FAST_URL = getURL('slow?0');
  var NEVER_FINISH_URL = getURL('download-known-size');
  var ERROR_GENERIC = downloads.ERROR_GENERIC;
  var ERROR_INVALID_URL = downloads.ERROR_INVALID_URL;
  var ERROR_INVALID_OPERATION = downloads.ERROR_INVALID_OPERATION;

  chrome.test.runTests([
    // TODO(benjhayden): Test onErased using remove().
    function downloadFilename() {
      downloads.download(
          {'url': SAFE_FAST_URL, 'filename': 'foo'},
          chrome.test.callbackPass(function(id) {
            chrome.test.assertEq(getNextId(), id);
          }));
      // TODO(benjhayden): Test the filename using onChanged.
    },
    function downloadOnCreated() {
      chrome.test.listenOnce(downloads.onCreated,
                             chrome.test.callbackPass(function(item) {}));
      downloads.download(
          {'url': SAFE_FAST_URL},
          function(id) {
            chrome.test.assertEq(getNextId(), id);
          });
    },
    function downloadSubDirectoryFilename() {
      downloads.download(
          {'url': SAFE_FAST_URL, 'filename': 'foo/slow'},
          chrome.test.callbackPass(function(id) {
            chrome.test.assertEq(getNextId(), id);
          }));
      // TODO(benjhayden): Test the filename using onChanged.
    },
    function downloadInvalidFilename() {
      downloads.download(
          {'url': SAFE_FAST_URL, 'filename': '../../../../../etc/passwd'},
          chrome.test.callbackFail(ERROR_GENERIC));
      // TODO(benjhayden): Give a better error message.
    },
    function downloadEmpty() {
      assertThrows(('Invalid value for argument 1. Property \'url\': ' +
                    'Property is required.'),
                   downloads.download, {});
    },
    function downloadInvalidSaveAs() {
      assertThrows(('Invalid value for argument 1. Property \'saveAs\': ' +
                    'Expected \'boolean\' but got \'string\'.'),
                   downloads.download,
                   {'url': SAFE_FAST_URL, 'saveAs': 'GOAT'});
    },
    function downloadInvalidHeadersOption() {
      assertThrows(('Invalid value for argument 1. Property \'headers\': ' +
                    'Expected \'array\' but got \'string\'.'),
                   downloads.download,
                   {'url': SAFE_FAST_URL, 'headers': 'GOAT'});
    },
    function downloadInvalidURL() {
      downloads.download(
          {'url': 'foo bar'},
          chrome.test.callbackFail(ERROR_INVALID_URL));
    },
    function downloadInvalidMethod() {
      assertThrows(('Invalid value for argument 1. Property \'method\': ' +
                    'Value must be one of: [GET, POST].'),
                   downloads.download,
                   {'url': SAFE_FAST_URL, 'method': 'GOAT'});
    },
    function downloadSimple() {
      downloads.download(
          {'url': SAFE_FAST_URL},
          chrome.test.callbackPass(function(id) {
            chrome.test.assertEq(getNextId(), id);
          }));
    },
    function downloadPost() {
      downloads.download(
          {'url': getURL('files/post/downloads/a_zip_file.js'),
           'method': 'POST',
           'body': 'WOOHOO'},
          chrome.test.callbackPass(function(id) {
              chrome.test.assertEq(getNextId(), id);
          }));
    },
    function downloadHeader() {
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'Foo', 'value': 'bar'}]
          },
          chrome.test.callbackPass(function(id) {
            chrome.test.assertEq(getNextId(), id);
          }));
    },
    function downloadInterrupted() {
      // TODO(benjhayden): Find a suitable URL and test that this id is
      // eventually interrupted using onChanged.
      downloads.download(
          {'url': SAFE_FAST_URL},
          chrome.test.callbackPass(function(id) {
            chrome.test.assertEq(getNextId(), id);
          }));
    },
    function downloadInvalidHeader() {
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{ 'name': 'Cookie', 'value': 'fake'}]
        },
        chrome.test.callbackFail(ERROR_GENERIC));
      // TODO(benjhayden): Give a better error message.
    },
    function downloadGetFileIconInvalidOptions() {
      assertThrows(('Invalid value for argument 2. Property \'cat\': ' +
                    'Unexpected property.'),
                   downloads.getFileIcon,
                   -1, {cat: 'mouse'});
    },
    function downloadGetFileIconInvalidSize() {
      assertThrows(('Invalid value for argument 2. Property \'size\': ' +
                    'Value must be one of: [16, 32].'),
                   downloads.getFileIcon, -1, {size: 31});
    },
    function downloadGetFileIconInvalidId() {
      downloads.getFileIcon(-42, {size: 32},
        chrome.test.callbackFail(ERROR_INVALID_OPERATION));
    },
    function downloadNoComplete() {
      // This is used partly to test cleanUp.
      downloads.download(
          {'url': NEVER_FINISH_URL},
          chrome.test.callbackPass(function(id) {
            chrome.test.assertEq(getNextId(), id);
          }));
    },
    function downloadPauseInvalidId() {
      downloads.pause(-42, chrome.test.callbackFail(ERROR_INVALID_OPERATION));
    },
    function downloadPauseInvalidType() {
      assertThrows(('Invalid value for argument 1. Expected \'integer\' ' +
                    'but got \'string\'.'),
                   downloads.pause,
                   'foo');
    },
    function downloadResumeInvalidId() {
      downloads.resume(-42, chrome.test.callbackFail(ERROR_INVALID_OPERATION));
    },
    function downloadResumeInvalidType() {
      assertThrows(('Invalid value for argument 1. Expected \'integer\' ' +
                    'but got \'string\'.'),
                   downloads.resume,
                   'foo');
    },
    function downloadCancelInvalidId() {
      // Canceling a non-existent download is not considered an error.
      downloads.cancel(-42, chrome.test.callbackPass(function() {}));
    },
    function downloadCancelInvalidType() {
      assertThrows(('Invalid value for argument 1. Expected \'integer\' ' +
                    'but got \'string\'.'),
                   downloads.cancel, 'foo');
    },
    function cleanUp() {
      // cleanUp must come last. It clears out all in-progress downloads
      // so the browser can shutdown cleanly.
      for (var id = 0; id < nextId; ++id) {
        downloads.cancel(id, chrome.test.callbackPass(function() {}));
      }
    }
  ]);
});
