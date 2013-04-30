// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';
embedder.webview = null;

embedder.setUp_ = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/files/extensions/platform_apps/web_view/console_messages' +
      '/guest.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpAndLoadGuest_ = function(doneCallback) {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  embedder.webview = document.querySelector('webview');
  if (!embedder.webview) {
    chrome.test.fail('No <webview> element created');
    return;
  }

  // Step 1. loadstop.
  embedder.webview.addEventListener('loadstop', function(e) {
    embedder.webview.contentWindow.postMessage(
        JSON.stringify(['create-channel']), '*');
  });

  // Step 2. Receive postMessage.
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'channel-created') {
      window.removeEventListener('message', onPostMessageReceived);
      doneCallback();
    } else {
      chrome.test.log('Unexpected response from guest');
      chrome.test.fail();
    }
  };
  window.addEventListener('message', onPostMessageReceived);

  embedder.webview.setAttribute('src', embedder.guestURL);
};

embedder.testLogHelper_ = function(id, expectedLogLevel, expectedLogMessage) {
  var called = false;
  var logCallback = function(e) {
    embedder.webview.removeEventListener('consolemessage', logCallback);
    chrome.test.assertEq(expectedLogLevel, e.level);
    chrome.test.assertEq(expectedLogMessage, e.message);
    chrome.test.succeed();
  };
  embedder.webview.addEventListener('consolemessage', logCallback);
  embedder.webview.contentWindow.postMessage(JSON.stringify([id]), '*');
};

// Tests.
embedder.tests.testLogInfo = function testLogInfo() {
  embedder.testLogHelper_('test-1', 0, 'log-one');
};

embedder.tests.testLogWarn = function testLogWarn() {
  embedder.testLogHelper_('test-2', 1, 'log-two');
}

embedder.tests.testLogError = function testLogError() {
  embedder.testLogHelper_('test-3', 2, 'log-three');
};

embedder.tests.testLogDebug = function testLogDebug() {
  embedder.testLogHelper_('test-4', -1, 'log-four');
};

embedder.tests.testThrow = function testThrow() {
  embedder.testLogHelper_('test-throw', 2, 'Uncaught Error: log-five');
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp_(config);
    embedder.setUpAndLoadGuest_(function() {
      chrome.test.runTests([
        embedder.tests.testLogInfo,
        embedder.tests.testLogWarn,
        embedder.tests.testLogError,
        embedder.tests.testLogDebug,
        embedder.tests.testThrow
      ]);
    });
  });
};
