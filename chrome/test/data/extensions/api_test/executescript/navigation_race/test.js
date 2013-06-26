// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var path = "/extensions/test_file.txt";
  var urlA = "http://a.com:" + config.testServer.port + path;
  var urlB = "http://b.com:" + config.testServer.port + path;
  var testTabId;

  function onTabUpdated(tabId, changeInfo, tab) {
    if (testTabId == tab.id && tab.status == "complete") {
      chrome.tabs.onUpdated.removeListener(onTabUpdated);
      chrome.tabs.update(tabId, {url: urlB});
      executeCodeInTab(testTabId, function() {
        // Generally, the tab navigation hasn't happened by the time we execute
        // the script, so it's still showing a.com, where we don't have
        // permission to run it.
        if (chrome.runtime.lastError) {
          chrome.test.assertTrue(
              chrome.runtime.lastError.message.indexOf(
                  'Cannot access contents of url "http://a.com:') == 0);
          chrome.test.notifyPass();
        } else {
          // If there were no errors, then the script did run, but it should
          // have run on on b.com (where we do have permission).
          chrome.tabs.get(tabId, function(tab) {
            chrome.test.assertTrue(
                tab.title.indexOf("hi, I'm on http://b.com:") == 0);
            chrome.test.notifyPass();
          });
        }
      });
    }
  }

  chrome.tabs.onUpdated.addListener(onTabUpdated);
  chrome.tabs.create({url: urlA}, function(tab) {
    testTabId = tab.id;
  });
});
