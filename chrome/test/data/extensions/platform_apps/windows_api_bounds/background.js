// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create("main.html", function(win){
    // Make sure we get back a bounds and that it isn't all 0's.
    var bounds = win.getBounds();
    if (!bounds ||
        (bounds.top == 0 && bounds.left == 0 &&
         bounds.width == 0 && bounds.height == 0)) {
      console.error("invalid bounds after app.window.create:" +
                    JSON.stringify(bounds));
      win.close();
      window.close();
    } else {
      chrome.test.sendMessage("background_ok");
    }
  });
});
