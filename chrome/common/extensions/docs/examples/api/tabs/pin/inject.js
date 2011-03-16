// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
window.addEventListener("keydown", function(event) {
  // Bind to both command (for Mac) and control (for Win/Linux)
  var modifier = event.ctrlKey || event.metaKey;
  if (modifier && event.shiftKey && event.keyCode == 80) {
    // Send message to background page to toggle tab
    chrome.extension.sendRequest({toggle_pin: true}, function(response) {
      // Do stuff on successful response
    });
  }
}, false);
