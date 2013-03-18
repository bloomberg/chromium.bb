// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.sendMessage("Launched");
  // Create a panel window first
  chrome.app.window.create(
    'main.html', { type: "panel" },
    function (win) {
      // Set the panel window icon
      win.setIcon("icon64.png")
      // Create the shell window; it should use the app icon, and not affect
      // the panel icon.
      chrome.app.window.create(
        'main.html', { type: "shell" },
        function (win) {
          chrome.test.sendMessage("Completed");
        });
    });
});
