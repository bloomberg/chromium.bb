// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Reads a blob content.
// @param {!Blob} blob The blob to read.
// @param {function(?string)} callback Called with the read blob content.
//     the content will be null on error.
function readBlob(blob, callback) {
  var reader = new FileReader();
  reader.onerror = function() { callback(null); };
  reader.onloadend = function() {
    callback(reader.result);
  }
  reader.readAsText(blob)
}

// Invokes |callback| with |returnValue| and verified a subsequent callback
// invocation throws an exception.
function wrapPrintCallback(callback, returnValue) {
  callback(returnValue);
  chrome.test.assertThrows(
      callback,
      ['OK'],
      'Event callback must not be called more than once.');
}

chrome.test.sendMessage('loaded', function(test) {
  chrome.test.runTests([function printTest() {
    if (test == 'NO_LISTENER') {
      chrome.test.sendMessage('ready');
      chrome.test.succeed();
      return;
    }

    chrome.printerProvider.onPrintRequested.addListener(function(job,
                                                                 callback) {
      chrome.test.assertFalse(!!chrome.printerProviderInternal);
      chrome.test.assertTrue(!!job);

      switch (test) {
        case 'IGNORE_CALLBACK':
          break;
        case 'ASYNC_RESPONSE':
          setTimeout(callback.bind(null, 'OK'), 0);
          break;
        case 'INVALID_VALUE':
          chrome.test.assertThrows(
              callback,
              ['XXX'],
              'Invalid value for argument 1. ' +
              'Value must be one of: ' +
              '[OK, FAILED, INVALID_TICKET, INVALID_DATA].');
          break;
        case 'FAILED':
        case 'INVALID_TICKET':
        case 'INVALID_DATA':
          wrapPrintCallback(callback, test);
          break;
        case 'OK':
          readBlob(job.document, function(content) {
            wrapPrintCallback(callback, !!content ? 'OK' : 'INVALID_DATA');

            if (content)
              chrome.test.assertEq('bytes', content);

            chrome.test.succeed();
          });

          // Test will end asynchronously.
          return;
        default:
          callback('FAILED');
          chrome.test.fail('Invalid input');
          return;
      }

      chrome.test.succeed();
    });

    chrome.test.sendMessage('ready');
  }]);
});
