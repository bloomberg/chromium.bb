// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function createWebview() {
  var webview = document.createElement('webview');
  document.body.appendChild(webview);
  return webview;
}

function onGetBackgroundExecuted(results) {
  chrome.send('testResult', [results.length == 1 && results[0] == 'red']);
};

function testExecuteScriptCode(url) {
  var webview = createWebview();

  var onSetBackgroundExecuted = function() {
    webview.executeScript({
      code: 'document.body.style.backgroundColor;'
    }, onGetBackgroundExecuted);
  };

  var onLoadStop = function() {
    webview.executeScript({
      code: 'document.body.style.backgroundColor = \'red\';'
    }, onSetBackgroundExecuted);
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.src = url;
}

function testExecuteScriptCodeFromFile(url) {
  var webview = createWebview();

  var onSetBackgroundExecuted = function() {
    webview.executeScript({
      code: 'document.body.style.backgroundColor;'
    }, onGetBackgroundExecuted);
  };

  var onLoadStop = function() {
    webview.executeScript({
      file: 'test/webview_execute_script.js'
    }, onSetBackgroundExecuted);
  };

  webview.addEventListener('loadstop', onLoadStop);
  webview.src = url;
}
