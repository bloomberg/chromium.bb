// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

onload = function() {
  chrome.test.getConfig(function(config) {
    chrome.tabs.create({"url": "about:blank"}, function(tab) {
      chrome.test.runTests([
          function onReplacedEvent() {
            var tabId = tab.id;

            var onReplaceListener = function(new_tab_id, old_tab_id) {
              chrome.test.assertTrue(tabId != new_tab_id);
              chrome.test.assertEq(tabId, old_tab_id);
              chrome.tabs.onReplaced.removeListener(onReplaceListener);
              chrome.test.succeed();
            };
            chrome.tabs.onReplaced.addListener(onReplaceListener);

            chrome.test.notifyPass();
          }
      ]);
    });
  });
};