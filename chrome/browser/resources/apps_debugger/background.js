// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mainWindow = null;
chrome.app.runtime.onLaunched.addListener(function() {
  if (mainWindow && !mainWindow.contentWindow.closed) {
    mainWindow.focus();
  } else {
    chrome.app.window.create('main.html', {
      width: 768,
      height: 1024,
    }, function(win) {
      mainWindow = win;
    });
  }
});
