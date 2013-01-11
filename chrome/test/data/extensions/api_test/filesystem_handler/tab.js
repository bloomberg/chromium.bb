// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a file intent handler and does the following during the test:

1. It first registers content hander.
2. When content handler callback is invoked, opens tab.html page and passes
   file url via hash ref.
3. Tries to open another file from the target file's filesystem (should fail).
4. Tries to resolve target file url and reads its content.
5. Send file content to file browser extension.
*/

// The ID of the extension we want to talk to.
var fileBrowserExtensionId = "ddammdhioacbehjngdmkjcjbnfginlla";

// Expected file content.
var expectedContent = null;

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

  chrome.runtime.sendMessage(fileBrowserExtensionId,
                               {fileContent: null,
                                error: {message: "File handler error: " + msg}},
                               function(response) {});
};

function onGotEntryByUrl(entry) {
  console.log('Got entry by URL: ' + entry.toURL());
  var reader = new FileReader();
  reader.onloadend = function(e) {
    if (reader.result != expectedContent) {
      chrome.runtime.sendMessage(
          fileBrowserExtensionId,
          {fileContent: null,
           error: {message: "File content does not match."}},
          function(response) {});
    } else {
      // Send data back to the file browser extension
      chrome.runtime.sendMessage(
          fileBrowserExtensionId,
          {fileContent: reader.result, error: null},
          function(response) {});
    }
  };
  reader.onerror = function(e) {
    errorCallback(reader.error);
  };
  entry.file(reader.readAsText.bind(reader), errorCallback);
};

function readEntryByUrl(entryUrl) {
  window.webkitResolveLocalFileSystemURL(entryUrl, onGotEntryByUrl,
                                         errorCallback);
};

// Try reading another file that has the same name as the entry we got from the
// executed task, but .log extension instead of .tXt.
// The .log file is created by fileBrowser component extension.
// We should not be able to get this file's fileEntry.
function tryOpeningLogFile(origEntry,successCallback, errorCallback) {
  var logFilePath = origEntry.fullPath.replace('.aBc', '.log');
  origEntry.filesystem.root.getFile(logFilePath, {},
                                    successCallback,
                                    errorCallback);
};

function onLogFileOpened(file) {
  errorCallback({message: "Opened file for which we don't have permission."});
};

// Try reading content of the received file.
function tryReadingReceivedFile(entry, evt) {
  var reader = new FileReader();
  reader.onloadend = function(e) {
    expectedContent = reader.result;
    readEntryByUrl(entry.toURL());
  };
  reader.onerror = function(e) {
    errorCallback(reader.error);
  };
  entry.file(reader.readAsText.bind(reader), errorCallback);
};

function runFileSystemHandlerTest(entries) {
  if (!entries || entries.length != 1 || !entries[0]) {
    chrome.runtime.sendMessage(
        fileBrowserExtensionId,
        {fileContent: null, error: "Invalid file entries."},
        function(response) {});
    return;
  }
  var entry = entries[0];

  // This operation should fail. If it does, we continue with testing.
  tryOpeningLogFile(entry, onLogFileOpened,
                    tryReadingReceivedFile.bind(this, entry));

};

function executeListener(id, details) {
  if (id != "AbcAction" && id != "BaseAction" && id != "123Action") {
    chrome.test.fail("Unexpected action id: " + id);
    return;
  }
  var file_entries = details.entries;
  if (!file_entries || file_entries.length != 1) {
    chrome.test.fail("Unexpected file url list");
    return;
  }
  chrome.tabs.get(details.tab_id, function(tab) {
    if (tab.title != "file browser component test") {
      chrome.test.fail("Unexpected tab title: " + tab.title);
      return;
    }
    runFileSystemHandlerTest(file_entries);
  });
}

chrome.fileBrowserHandler.onExecute.addListener(executeListener);
