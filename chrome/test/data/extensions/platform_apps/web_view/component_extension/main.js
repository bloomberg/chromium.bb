// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.baseGuestURL = '';
embedder.guestURL = '';

window.runTest = function(testName) {
  if (testName == 'testComponentExtension') {
    testComponentExtension();
  } else if (testName =='testNonComponentExtension') {
    testNonComponentExtension();
  } else {
    console.log('Incorrect testName: ' + testName);
    chrome.test.sendMessage('TEST_FAILED');
  }
};
// window.* exported functions end.

function testComponentExtension() {
  testExtension('to_component');
}

function testNonComponentExtension() {
  testExtension('to_non_component');
}

// Creates a webview with the |embedder.guestURL| and posts |messageToPost|
// to the webview's content window.
function testExtension(messageToPost) {
  var onPostMessageReceived = function(e) {
    if (e.data == 'SUCCESS') {
      chrome.test.sendMessage('TEST_PASSED');
    } else {
      chrome.test.sendMessage('TEST_FAILED');
    }
  };

  var loaded = false;
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview style="width: 100px; height: 100px;"' +
      ' src="' + embedder.guestURL + '"' +
      '></webview>';
  var webview = document.querySelector('webview');

  if (!webview) {
    chrome.test.fail('No <webview> element created');
    return null;
  }

  webview.addEventListener('loadstop', function(e) {
    if (!loaded) {
      loaded = true;
      window.addEventListener('message', onPostMessageReceived);
      webview.contentWindow.postMessage(messageToPost, '*');
    }
  });
}

onload = function() {
  console.log(JSON.stringify(chrome));
  chrome.test.getConfig(function(config) {
    embedder.baseGuestURL = 'http://localhost:' + config.testServer.port;
    embedder.guestURL =
        embedder.baseGuestURL +
        '/extensions/platform_apps/web_view/component_extension/guest.html';
    chrome.test.sendMessage('Launched');
  });
};
