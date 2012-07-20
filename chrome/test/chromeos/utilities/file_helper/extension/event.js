// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if(chrome.fileBrowserHandler) {
  chrome.fileBrowserHandler.onExecute.addListener(
    function(id, details) {
      if (id == 'view')
        OnExecute(details.entries);
     });
}

function OnExecute(entries) {
  if (!entries || !entries[0])
    return;

  chrome.tabs.create({
    url: "test.html?file=" + escape(entries[0].fullPath)
  });
}

