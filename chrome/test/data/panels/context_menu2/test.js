// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onclick(info) {
  chrome.test.sendMessage("ow, don't do that!");
}

chrome.contextMenus.create({"title":"Extension Item Bogey",
                            "onclick": onclick}, function() {
  if (!chrome.runtime.lastError) {
    chrome.test.sendMessage("created bogey item");
  }
});
