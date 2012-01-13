// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.extension.onRequest.addListener(
  function(request, sender, sendResponse) {
    if (request.toggle_pin) {
      // Get the currently selected tab
      chrome.tabs.getSelected(null, function(tab) {
        // Toggle the pinned status
        chrome.tabs.update(tab.id, {'pinned': !tab.pinned});
      });
    }
  }
);
