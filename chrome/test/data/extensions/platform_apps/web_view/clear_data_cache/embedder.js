// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var LOG = function(msg) { window.console.log(msg); };

function ClearDataTester() {
  this.webview_ = null;
  this.id_ = '';

  this.inlineClickCalled_ = false;
  this.globalClickCalled_ = false;

  // Used for createThreeMenuItems().
  this.numItemsCreated_ = 0;

  this.failed_ = false;
}

ClearDataTester.prototype.setWebview = function(webview) {
  this.webview_ = webview;
};

ClearDataTester.prototype.testClearDataCache = function() {
  this.webview_.clearData(
      {since: 10}, {"cache": true}, function doneCallback() {
        LOG('clearData done');
        chrome.test.sendMessage('WebViewTest.CLEAR_DATA_DONE');
      });
};

var tester = new ClearDataTester();

// window.* exported functions begin.
window.testClearDataCache = function() {
  LOG('window.testClearDataCache');
  tester.testClearDataCache();
};
// window.* exported functions end.

function setUpTest(messageCallback) {
  var guestUrl = 'data:text/html,<html><body>guest</body></html>';
  var webview = document.createElement('webview');

  webview.onloadstop = function(e) {
    LOG('webview has loaded.');
    webview.executeScript(
      {file: 'guest.js'},
      function(results) {
        if (!results || !results.length) {
          chrome.test.sendMessage('WebViewTest.FAILURE');
          return;
        }
        LOG('Script has been injected into webview.');
        // Establish a communication channel with the guest.
        var msg = ['connect'];
        webview.contentWindow.postMessage(JSON.stringify(msg), '*');
      });
  };

  window.addEventListener('message', function(e) {
    var data = JSON.parse(e.data);
    if (data[0] == 'connected') {
      console.log('A communication channel has been established with webview.');
    }
    messageCallback(webview);
  });

  webview.setAttribute('src', guestUrl);
  document.body.appendChild(webview);
}

onload = function() {
  chrome.test.getConfig(function(config) {
    setUpTest(function(webview) {
      LOG('Guest load completed.');
      chrome.test.sendMessage('WebViewTest.LAUNCHED');
      tester.setWebview(webview);
    });
  });
};
