// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.lastDropData = 'Uninitialized';

// Called from web_view_interactive_ui_tests.cc.
window.getLastDropData = function() {
  return window.lastDropData;
};

var startTest = function() {
  window.addEventListener('message', onPostMessageReceived, false);
  var webview = document.getElementById('webview');
  webview.addEventListener('loadstop', function(e) {
    webview.contentWindow.postMessage(
        JSON.stringify(['create-channel']), '*');
  });
};

var onPostMessageReceived = function(e) {
  window.console.log('embedder.onPostMessageReceived');
  var data = JSON.parse(e.data);
  window.console.log('data: ' + data);
  switch (data[0]) {
    case 'connected':
      chrome.test.sendMessage('connected');
      break;
    case 'guest-got-drop':
      window.lastDropData = data[1];
      chrome.test.sendMessage('guest-got-drop');
      break;
    default:
      window.console.log('ERR: curious message received in emb: ' + data);
      break;
  }
};

chrome.test.getConfig(function(config) {
  var guestURL = 'http://localhost:' + config.testServer.port +
      '/extensions/platform_apps/web_view/dnd_within_webview/guest.html';
  document.querySelector('#webview-tag-container').innerHTML =
      '<webview id=\'webview\' style="width: 300px; height: 150px; ' +
      'margin: 0; padding: 0;"' +
      ' src="' + guestURL + '"' +
      '></webview>';
  startTest();
});
