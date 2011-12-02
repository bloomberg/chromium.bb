// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onCreated.addListener(function() {
  chrome.tabs.onCreated.removeListener(arguments.callee);
  chrome.extension.getBackgroundPage().verifyCreatedTab.apply(null, arguments);
});
window.open('foo.html');
