// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setUpWebView(embedder) {
  var webview = document.createElement('webview');
  embedder.appendChild(webview);
  chrome.test.getConfig(function(config) {
    var url = 'http://localhost:' + config.testServer.port
        + '/extensions/platform_apps/web_view/focus_visibility/guest.html';
    webview.onloadstop = function() {
      chrome.test.sendMessage('WebViewInteractiveTest.WebViewInitialized');
    };
    webview.src = url;
    console.log('Setting URL to "' + url + '".');
  });
}

function reset() {
  getWebView().style.visibility = 'visible';
  document.querySelector('button').focus();
  webViewButtonReceivedFocus = false;
}

function sendMessageToWebViewAndReceiveReply(message, replyCallback) {
  function callback(e) {
    window.removeEventListener('message', callback);
    replyCallback(e.data);
  }
  if (replyCallback) {
    window.addEventListener('message', callback);
  }
  getWebView().contentWindow.postMessage(message, '*');
}

function getWebView() {
  return document.querySelector('webview');
}

window.onAppMessage = function(command) {
  switch (command) {
    case 'init':
      setUpWebView(document.querySelector('div'));
      break;
    case 'reset':
      reset();
      sendMessageToWebViewAndReceiveReply("reset", function() {
        chrome.test.sendMessage('WebViewInteractiveTest.DidReset');
      });
      break;
    case 'verify':
      sendMessageToWebViewAndReceiveReply('verify', function(result) {
        chrome.test.sendMessage(result === 'was-focused' ?
            'WebViewInteractiveTest.WebViewButtonWasFocused' :
            'WebViewInteractiveTest.WebViewButtonWasNotFocused');
      });
      break;
    case 'hide-webview':
      getWebView().style.visibility = 'hidden';
      chrome.test.sendMessage('WebViewInteractiveTest.DidHideWebView');
      break;
  }
}
window.onload = function() {
  chrome.test.sendMessage('WebViewInteractiveTest.LOADED');
}
