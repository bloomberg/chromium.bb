// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See chrome/browser/extensions/web_view_interactive_browsertest.cc
// (WebViewInteractiveTest, PointerLock) for documentation on this test.

var startTest = function() {
  window.addEventListener("message", receiveMessage, false);
  chrome.test.sendMessage('guest-loaded');
  var webview = document.getElementById('webview');
  webview.addEventListener('loadstop', function(e) {
    webview.contentWindow.postMessage("msg", "*");
  });
};
var fail = false;
var receiveMessage = function(event) {
  if (event.data == 'Pointer was not locked to locktarget1.') {
    fail = true;
    chrome.test.fail(event.data);
  } else if (event.data == 'delete me') {
    var webview_container = document.getElementById('webview-tag-container');
    webview_container.parentNode.removeChild(webview_container);
    setTimeout(function () { chrome.test.sendMessage('timeout'); }, 5000);
    document.getElementById('mousemove-capture-container').addEventListener(
        "mousemove", function (e) {
      console.log('move-captured');
      chrome.test.sendMessage('move-captured');


    });
    console.log('deleted');
    chrome.test.sendMessage('deleted');
  }
  if (!fail)
    chrome.test.sendMessage(event.data);
}

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/files/extensions/platform_apps/web_view/pointer_lock/guest.html';
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview id=\'webview\' style="width: 400px; height: 400px; ' +
      'margin: 0; padding: 0;"' +
      ' src="' + guestURL + '"' +
      '></webview>';
  startTest();
});
