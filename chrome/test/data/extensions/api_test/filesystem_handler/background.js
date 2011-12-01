// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var last_file_entries = null;

function getLastFileEntries() {
  return last_file_entries;
}

function executeListener(id, details) {
  if (id != "TextAction" && id != "BaseAction" && id != "JpegAction") {
    chrome.test.fail("Unexpected action id: " + id);
    return;
  }
  var file_entries = details.entries;
  if (!file_entries || file_entries.length != 1) {
    chrome.test.fail("Unexpected file url list");
    return;
  }
  last_file_entries = file_entries;
  chrome.tabs.get(details.tab_id, function(tab) {
    if (tab.title != "file browser component test") {
      chrome.test.fail("Unexpected tab title: " + tab.title);
      return;
    }
    // Create a new tab
    chrome.tabs.create({
      url: "tab.html"
    });
  });
}

chrome.fileBrowserHandler.onExecute.addListener(executeListener);

// This extension just initializes its chrome.fileBrowserHandler.onExecute
// event listener, the real testing is done when this extension's handler is
// invoked from filebrowser_component tests. This event will be raised from that
// component extension test and it simulates user action in the file browser.
// tab.html part of this extension can run only after the component raises this
// event, since that operation sets the propery security context and creates
// event's payload with proper file Entry instances. tab.html will return
// results of its execution to filebrowser_component test through a
// cross-component message.
chrome.test.succeed();
