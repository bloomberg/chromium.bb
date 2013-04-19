// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.tests = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

embedder.setUp = function(config) {
  embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
  embedder.guestURL = embedder.baseGuestURL +
      '/files/extensions/platform_apps/web_view/focus' +
      '/guest.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function() {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (!webview) {
    chrome.test.fail('No <webview> element created');
  }
  return webview;
};

/** @private */
embedder.waitForResponseFromGuest_ =
    function(webview,
             testName,
             channelCreationCallback,
             expectedResponse,
             responseCallback) {
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'channel-created') {
      channelCreationCallback(webview);
      return;
    }
    var name = data[1];
    if ((response != expectedResponse) || (name != testName)) {
      chrome.test.log('Unexpected response from guest.');
      return;
    }
    responseCallback();
  };
  window.addEventListener('message', onPostMessageReceived);

  var onWebViewLoadStop = function(e) {
    // This creates a communication channel with the guest.
    webview.contentWindow.postMessage(
        JSON.stringify(['create-channel', testName]), '*');
  };
  webview.addEventListener('loadstop', onWebViewLoadStop);
  webview.setAttribute('src', embedder.guestURL);
};

// Tests begin.

// The embedder has to initiate a post message so that the guest can get a
// reference to embedder to send the reply back.

embedder.testFocus_ = function(testName,
                               channelCreationCallback,
                               expectedResponse,
                               responseCallback) {
  var webview = embedder.setUpGuest_();

  embedder.waitForResponseFromGuest_(webview,
                                     testName,
                                     channelCreationCallback,
                                     expectedResponse,
                                     responseCallback);
};

embedder.tests.testFocusEvent = function testFocusEvent() {
  var seenResponse = false;
  embedder.testFocus_('testFocusEvent', function(webview) {
    webview.focus();
  }, 'focused', function() {
    // The focus event fires three times on first focus. We only care about
    // the first focus.
    if (seenResponse)
      return;
    seenResponse = true;
    chrome.test.succeed();
  });
}

embedder.tests.testBlurEvent = function testBlurEvent() {
  var seenResponse = false;
  embedder.testFocus_('testFocusEvent', function(webview) {
    webview.focus();
  }, 'blurred', function() {
    chrome.test.succeed();
  });
}

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    chrome.test.runTests([
      embedder.tests.testFocusEvent,
      embedder.tests.testBlurEvent
    ]);
  });
};
