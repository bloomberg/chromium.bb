// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kPortErrorMessage =
    'Could not establish connection. Receiving end does not exist.';

// onMessage / onConnect in the same frame cannot be triggered by sendMessage or
// connect, so both attempts to send a message should fail with an error.

chrome.runtime.onMessage.addListener(function(msg, sender, sendResponse) {
  chrome.test.fail('onMessage should not be triggered. Received: ' + msg);
});

chrome.runtime.onConnect.addListener(function(port) {
  chrome.test.fail('onConnect should not be triggered. Port: ' + port.name);
});

chrome.test.runTests([
  function sendMessageExpectingNoAnswer() {
    chrome.runtime.sendMessage('hello without callback');
    // The timer is here to try and get the test failure in the correct test
    // case (namely "sendMessageExpectingNoAnswer"). If the timer is too short,
    // but the test fails, then onMessage will still print an error that shows
    // which test fails, but the test runner will think that it is running in
    // the next test, and attribute the failure incorrectly.
    setTimeout(chrome.test.callbackPass(), 100);
  },

  function sendMessageExpectingNoAnswerWithCallback() {
    chrome.runtime.sendMessage('hello with callback',
        chrome.test.callbackFail(kPortErrorMessage));
  },

  function connectAndDisconnect() {
    chrome.runtime.connect({ name: 'The First Port'}).disconnect();
    // Like sendMessageExpectingNoAnswer; onConnect should not be triggered.
    setTimeout(chrome.test.callbackPass(), 100);
  },

  function connectExpectDisconnect() {
    chrome.runtime.connect({ name: 'The Last Port'}).onDisconnect.addListener(
        chrome.test.callbackFail(kPortErrorMessage));
  },
]);
