// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

      if (test == 'IGNORE_CALLBACK') {
        chrome.test.succeed();
        return;
      }

      if (test == 'ASYNC_RESPONSE') {
        setTimeout(callback.bind(null, 'OK'), 0);
        chrome.test.succeed();
        return;
      }

      if (test == 'INVALID_VALUE') {
        chrome.test.assertThrows(
            callback,
            ['XXX'],
            'Invalid value for argument 1. ' +
            'Value must be one of: ' +
            '[OK, FAILED, INVALID_TICKET, INVALID_DATA].');
      } else {
        chrome.test.assertTrue(test == 'OK' || test == 'FAILED' ||
            test == 'INVALID_TICKET' || test == 'INVALID_DATA');
        callback(test);
      }

      chrome.test.assertThrows(
          callback,
          [test],
          'Event callback must not be called more than once.');

      chrome.test.succeed();
    });

    chrome.test.sendMessage('ready');
  }]);
});
