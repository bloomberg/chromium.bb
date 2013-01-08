// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a file intent handler with transient background page and does
the following during the test:

1. It first registers content hander.
2. When content handler callback is invoked, opens tab.html page and passes
   file url via persistent localStorage var.
3. Tries to resolve target file url and reads its content.
4. Send file content to file browser extension.
*/

function errorCallback(error) {
  var msg = '';
  if (!error.code) {
    msg = error.message;
  } else {
    switch (error.code) {
      case FileError.QUOTA_EXCEEDED_ERR:
        msg = 'QUOTA_EXCEEDED_ERR';
        break;
      case FileError.NOT_FOUND_ERR:
        msg = 'NOT_FOUND_ERR';
        break;
      case FileError.SECURITY_ERR:
        msg = 'SECURITY_ERR';
        break;
      case FileError.INVALID_MODIFICATION_ERR:
        msg = 'INVALID_MODIFICATION_ERR';
        break;
      case FileError.INVALID_STATE_ERR:
        msg = 'INVALID_STATE_ERR';
        break;
      default:
        msg = 'Unknown Error';
        break;
    };
  }

  // The ID of the extension we want to talk to.
  var fileBrowserExtensionId = "ddammdhioacbehjngdmkjcjbnfginlla";

  chrome.runtime.sendMessage(fileBrowserExtensionId,
                               {fileContent: null,
                                error: {message: "File handler error: " + msg}},
                               function(response) {});
};

function runFileSystemHandlerTest(entries) {
  if (!entries || entries.length != 1 || !entries[0]) {
    errorCallback({message: "Invalid file entries"});
    return;
  }
  localStorage.entryURL = entries[0].toURL();

  chrome.tabs.create({url: 'tab.html'});
};

chrome.fileBrowserHandler.onExecute.addListener(function(id, details) {
  if (id != "AbcAction" && id != "BaseAction" && id != "123Action") {
    errorCallback({message: "Unexpected action id: " + id});
    return;
  }
  var file_entries = details.entries;
  if (!file_entries || file_entries.length != 1) {
    errorCallback({message: "Unexpected file url list"});
    return;
  }
  chrome.tabs.get(details.tab_id, function(tab) {
    if (tab.title != "file browser component test") {
      errorCallback({message: "Unexpected tab title: " + tab.title});
      return;
    }
    runFileSystemHandlerTest(file_entries);
  });
});
