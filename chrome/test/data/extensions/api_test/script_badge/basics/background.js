// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called when the user clicks on the script badge.
chrome.scriptBadge.onClicked.addListener(function(windowId) {
  chrome.test.notifyPass();
});

chrome.webNavigation.onCompleted.addListener(function(details) {
    if (details.url.indexOf("http://") === 0) {
      // Executing script causes an icon animation to run. Originally, if
      // this animation was still running when the browser shut down, it
      // would cause a crash due to the Extension being destroyed on the
      // IO thread, and transitively causing a UI-only object to be
      // destroyed there.
      chrome.tabs.executeScript(
          details.tabId,
          {
            // The setTimeout allows the ScriptBadgeController to
            // reliably notice that a content script has run.
            "code": ("window.setTimeout(function() {" +
                     "  chrome.test.notifyPass();}, 1)")
          });
    }
  });

chrome.test.notifyPass();
