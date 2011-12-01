// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var baseUrl = 'http://a.com:PORT/files/extensions/api_test/executescript/' +
    'frame_after_load/';

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status != 'complete')
    return;

  chrome.test.runTests([
    function() {
      // Tests that we can still execute scripts after a frame has loaded after
      // the main document has completed.
      var injectFrameCode = 'var frame = document.createElement("iframe");' +
          'frame.src = "' + baseUrl + 'inner.html";' +
          'frame.onload = function() {chrome.extension.connect().postMessage("loaded")};' +
          'document.body.appendChild(frame)';
      var postFrameCode = 'chrome.extension.connect().postMessage("done");';

      chrome.self.onConnect.addListener(function(port) {
        port.onMessage.addListener(function(data) {
          if (data == 'loaded') {
            chrome.tabs.executeScript(tabId, {code: postFrameCode});
          } else if (data == 'done') {
            chrome.test.succeed();
          }
        });
      });
      chrome.tabs.executeScript(tabId, {code: injectFrameCode});
    }
  ]);
});

chrome.test.getConfig(function(config) {
  baseUrl = baseUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.create({ url: baseUrl + 'outer.html' });
});
