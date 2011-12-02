// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.pageActions["action"].addListener(function(actionId, info) {
  if (actionId == "action" && typeof(info.tabId) == "number" &&
      typeof(info.tabUrl) == "string" && info.button == 1) {
    chrome.test.notifyPass();
  } else {
    chrome.test.notifyFail("pageActions listener failed");
  }
});

chrome.test.notifyPass();
