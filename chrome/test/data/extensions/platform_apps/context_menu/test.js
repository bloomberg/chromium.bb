// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.contextMenus.create({title: 'Extension Item 1',
                             id: 'id1'}, function() {
    chrome.contextMenus.create({title: 'Extension Item 2',
                               id: 'id2'}, function() {
      chrome.app.window.create('main.html', {}, function() {});
    });
  });
});
