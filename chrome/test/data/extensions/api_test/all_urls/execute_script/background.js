// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  chrome.tabs.executeScript(tabId, {file: "execute_script.js"});
});

chrome.extension.onRequest.addListener(
  function(request, sender, sendResponse) {
    // Let the extension know where the script ran.
    var url = sender.tab ? sender.tab.url : 'about:blank';
    chrome.test.sendMessage('execute: ' + url);
  });
