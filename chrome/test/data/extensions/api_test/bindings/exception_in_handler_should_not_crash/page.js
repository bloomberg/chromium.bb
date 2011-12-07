// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.create({}, function() {
  throw new Error("tada");
});
chrome.tabs.create({}, function() {
  chrome.test.notifyPass();
});
