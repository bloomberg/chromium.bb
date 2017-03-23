// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = {};
embedder.guestURL = '';

window.runTest = function(testName) {
  if (testName == 'testLoadWebviewAccessibleResource') {
    testLoadWebviewAccessibleResource();
  } else if (testName == 'testReloadWebviewAccessibleResource') {
    testReloadWebviewAccessibleResource();
  } else {
    window.console.log('Incorrect testName: ' + testName);
    chrome.test.sendMessage('TEST_FAILED');
  }
}

function testLoadWebviewAccessibleResource() {
  var webview = document.querySelector('webview');

  webview.addEventListener('loadstop', function() {
    webview.executeScript(
        {code: "document.querySelector('img').naturalWidth"}, function(result) {
          // If the test image loads successfully, it will have a |naturalWidth|
          // of 17, and the test passes.
          if (result[0] == 17)
            chrome.test.sendMessage('TEST_PASSED');
          else
            chrome.test.sendMessage('TEST_FAILED');
        });
  });

  webview.src = embedder.guestURL;
};

function testReloadWebviewAccessibleResource() {
  var webview = document.querySelector('webview');
  var didReload = false;

  webview.addEventListener('loadstop', function() {
    if (didReload) {
      // Check that the webview loaded the content correctly.
      webview.executeScript(
          {code: 'document.body.innerText'}, function(result) {
            if (result == 'Foo')
              chrome.test.sendMessage('TEST_PASSED');
            else {
              console.log('webview content is incorrect: ' + result);
              chrome.test.sendMessage('TEST_FAILED');
            }
          });
    } else {
      webview.executeScript({code: 'location.reload();'});
      didReload = true;
    }
  });
  webview.src = '/assets/foo.html';
}

onload = function() {
  chrome.test.getConfig(function(config) {
    embedder.guestURL =
        'http://localhost:' + config.testServer.port +
        '/extensions/platform_apps/web_view/load_webview_accessible_resource/' +
        'guest.html';
    chrome.test.sendMessage('Launched');
  });
};
