// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {

  var screenWidth = screen.availWidth;
  var screenHeight = screen.availHeight;
  var width = 500;
  var height = 300;

  chrome.app.window.create('index.html', {
    bounds: {
      width: width,
      height: height,
      left: Math.round((screenWidth-width)/2),
      top: Math.round((screenHeight-height)/2)
    }
  });
});

chrome.runtime.onSuspend.addListener(function() {
  var now = new Date();
  chrome.storage.local.set({"last_save": now.toLocaleString()}, function() {
    console.log("Finished writing last_save: " + now.toLocaleString());
  });
});
