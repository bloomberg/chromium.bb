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
      '/files/extensions/platform_apps/web_view/execute_code' +
      '/guest.html';
  chrome.test.log('Guest url is: ' + embedder.guestURL);
};

/** @private */
embedder.setUpGuest_ = function(partitionName) {
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"></webview>';
  var webview = document.querySelector('webview');
  if (partitionName) {
    webview.partition = partitionName;
  }
  if (!webview) {
    chrome.test.fail('No <webview> element created');
  }
  return webview;
};

/** @private */
embedder.waitForResponseFromGuest_ =
    function(webview,
             channelCreationCallback,
             expectedResponse,
             responseCallback) {
  var hasNavigated = webview.hasAttribute('src');
  if (hasNavigated)
    channelCreationCallback();
  var onPostMessageReceived = function(e) {
    var data = JSON.parse(e.data);
    var response = data[0];
    if (response == 'channel-created') {
      channelCreationCallback();
      return;
    }
    if (response != expectedResponse) {
      return;
    }
    responseCallback(data);
    window.removeEventListener('message', onPostMessageReceived);
  };
  window.addEventListener('message', onPostMessageReceived);

  var onWebViewLoadStop = function(e) {
    // This creates a communication channel with the guest.
    webview.contentWindow.postMessage(
        JSON.stringify(['create-channel']), '*');
  };
  if (!hasNavigated) {
    webview.addEventListener('loadstop', onWebViewLoadStop);
    webview.setAttribute('src', embedder.guestURL);
  }
};

// Tests begin.

embedder.tests.testInsertCSS = function testInsertCSS() {
  var testName = 'testNewWindowExecuteScript';
  var webview = embedder.setUpGuest_('foobar');
  var testBackgroundColorAfterCSSInjection = function() {
    webview.insertCSS({file: 'guest.css'}, function (results) {
      // Verify that the background color is now red after injecting
      // the CSS file.
      embedder.waitForResponseFromGuest_(webview, function() {
        webview.contentWindow.postMessage(
            JSON.stringify(['get-style', 'background-color']), '*');
      }, 'style', function(data) {
        var propertyName = data[1];
        var value = data[2];
        chrome.test.assertEq('background-color', propertyName);
        chrome.test.assertEq('rgb(255, 0, 0)', value);
        chrome.test.succeed();
      });
    });
  };
  // Test the background color before CSS injection. Verify that the background
  // color is indeed white.
  embedder.waitForResponseFromGuest_(webview, function() {
    webview.contentWindow.postMessage(
        JSON.stringify(['get-style', 'background-color']), '*');
  }, 'style', function(data) {
    var propertyName = data[1];
    var value = data[2];
    chrome.test.assertEq('background-color', propertyName);
    chrome.test.assertEq('rgba(0, 0, 0, 0)', value);
    testBackgroundColorAfterCSSInjection();
  });
};

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.setUp(config);
    chrome.test.runTests([
      embedder.tests.testInsertCSS
    ]);
  });
};
