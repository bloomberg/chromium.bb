// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  // TODO(kcarattini): Check if the app is already running. If so, bring it
  // to focus rather than creating a new window.

  // TODO(kcarattini): Don't show the window until the launch mode has been
  // established.
  chrome.app.window.create('main.html', {
    'frame': 'none',
    'resizable': false,
    'bounds': {
      'width': 800,
      'height': 600
    }
  });
});
