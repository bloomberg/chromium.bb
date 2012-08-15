// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(data) {
  chrome.app.window.create('view_checks.html', {
    frame: 'chrome',
    width: 1024,
    height: 768,
    minWidth: 1024,
    minHeight: 768
  });
});
