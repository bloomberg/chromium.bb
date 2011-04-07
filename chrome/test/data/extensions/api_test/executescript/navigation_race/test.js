// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var path = "/files/extensions/test_file.txt";
  var urlA = "http://a.com:" + config.testServer.port + path;
  var urlB = "http://b.com:" + config.testServer.port + path;
  var testTabId;

  function onTabUpdated(tabId, changeInfo, tab) {
    if (testTabId == tab.id && tab.status == "complete") {
      chrome.tabs.onUpdated.removeListener(onTabUpdated);
      chrome.tabs.update(tabId, {url: urlB});
      executeCodeInTab(testTabId, function() {
        chrome.test.assertTrue(
            chrome.extension.lastError.message.indexOf(
                'Cannot access contents of url "http://a.com:') == 0);
        chrome.test.notifyPass();
      });
    }
  }

  chrome.tabs.onUpdated.addListener(onTabUpdated);
  chrome.tabs.create({url: urlA}, function(tab) {
    testTabId = tab.id;
  });
});
