// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function onMessage() {
      var expectedCalls = 2;
      var collapseKeyTested = false;
      var regularMessageTested = false;
      var eventHandler = function(message) {
        // Test with and without a collapse key.
        if (message.hasOwnProperty('collapseKey')) {
          chrome.test.assertEq('collapseKeyValue', message.collapseKey);
          collapseKeyTested = true;
        } else {
          chrome.test.assertFalse(message.hasOwnProperty('collapseKey'));
          regularMessageTested = true;
        }

        // The message is expected to carry data regardless of collapse key.
        chrome.test.assertEq(2, Object.keys(message.data).length);
        chrome.test.assertTrue(message.data.hasOwnProperty('property1'));
        chrome.test.assertTrue(message.data.hasOwnProperty('property2'));
        chrome.test.assertEq('value1', message.data.property1);
        chrome.test.assertEq('value2', message.data.property2);

        --expectedCalls;
        if (expectedCalls == 0) {
          chrome.gcm.onMessage.removeListener(eventHandler);
          if (collapseKeyTested && regularMessageTested) {
            chrome.test.succeed();
          } else {
            chrome.test.fail();
          }
        }
      };
      chrome.gcm.onMessage.addListener(eventHandler);
    }
  ]);
};
