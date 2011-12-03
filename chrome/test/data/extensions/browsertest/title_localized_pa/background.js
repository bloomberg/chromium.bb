// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.self.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(mybool) {
    // Let Chrome know that the PageAction needs to be enabled for this tabId
    // and for the url of this page.
    chrome.pageAction.show(port.tab.id);
  });
});
