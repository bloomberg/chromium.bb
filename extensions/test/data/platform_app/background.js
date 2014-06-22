// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stub for app_shell.
var createWindow =
    chrome.shell ? chrome.shell.createWindow : chrome.app.window.create;

chrome.app.runtime.onLaunched.addListener(function() {
  createWindow('hello.html', {
    'innerBounds': {
      'width': 400,
      'height': 300
    }
  });
});
