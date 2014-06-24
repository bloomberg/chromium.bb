// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var allTests = [
  function testCloseTab() {
    getUrlFromConfig(function(url) {
      chrome.tabs.create({'url': url}, function(tab) {
        chrome.automation.getTree(function(rootNode) {
          rootNode.addEventListener(EventType.destroyed, function() {
            // Should get a 'destroyed' event when the tab is closed.
            // TODO(aboxhall): assert actual cleanup has occurred once
            // crbug.com/387954 makes it possible.
            chrome.test.succeed();
          });
          chrome.tabs.remove(tab.id);
        });
      });
    });
  }
]
chrome.test.runTests(allTests);
