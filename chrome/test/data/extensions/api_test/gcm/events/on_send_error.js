// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.runTests([
    function onSendError() {
      var errorMessages = [
          'Asynchronous operation is pending.',
          'Server error occured.',
          'Network error occured.',
          'Unknown error occured.',
          'Time-to-live exceeded.'
        ];
      var messageIds = [
          'error_message_1',
          'error_message_2',
          'error_message_3',
          'error_message_4',
          'error_message_5'
        ];
      var currentError = 0;
      var eventHandler = function(error) {
        chrome.test.assertEq(errorMessages[currentError], error.errorMessage);
        chrome.test.assertEq(messageIds[currentError], error.messageId);
        currentError += 1;
        if (currentError == messageIds.length) {
          chrome.gcm.onSendError.removeListener(eventHandler);
          chrome.test.succeed();
        }
      };
      chrome.gcm.onSendError.addListener(eventHandler);
    }
  ]);
};
