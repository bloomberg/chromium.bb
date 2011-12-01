// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
var background_page = chrome.extension.getBackgroundPage();
chrome.tabs.getCurrent(function(tab) {
  assertEq(background_page.tabB, tab.id);
  background_page.infobarCallbackB();
});
