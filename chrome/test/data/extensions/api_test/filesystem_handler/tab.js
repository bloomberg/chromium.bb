// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a file intent handler and does the following during the test:

1. Its background page first registers content hander.
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

function errorCallback(e) {
  chrome.extension.sendRequest(fileBrowserExtensionId,
                               {fileContent: null, error: e},
                               function(response) {});
}

function onGotEntryByUrl(entry) {
  console.log('Got entry by URL: ' + entry.toURL());
  var reader = new FileReader();
  reader.onloadend = function(e) {
      if (reader.result != expectedContent) {
        chrome.extension.sendRequest(
            fileBrowserExtensionId,
            {fileContent: null,
             error: {message: "File content does not match."}},
            function(response) {});
      } else {
        // Send data back to the file browser extension
        chrome.extension.sendRequest(
            fileBrowserExtensionId,
            {fileContent: reader.result, error: null},
            function(response) {});
      }
    };
  reader.onerror = errorCallback;
  entry.file(function(file) {
    reader.readAsText(file);
  });
}

function readEntryByUrl(entryUrl) {
  window.webkitResolveLocalFileSystemURL(entryUrl, onGotEntryByUrl,
                                         errorCallback);
}

// Try reading another file that has the same name as the entry we got from the
// executed task, but .log extension instead of .tXt.
// The .log file is created by fileBrowser component extension.
// We should not be able to get this file's fileEntry.
function tryOpeningLogFile(origEntry,successCallback, errorCallback) {
  var logFilePath =origEntry.fullPath.replace('.tXt', '.log');
  origEntry.filesystem.root.getFile(logFilePath, {},
                                    successCallback,
                                    errorCallback);
};

function onLogFileOpened(file) {
  errorCallback({code: undefined,
                 message: "Opened file for which we don't have permission."});
};

// Try reading content of the received file.
function tryReadingReceivedFile(entry, error) {
  var reader = new FileReader();
  reader.onloadend = function(e) {
    var content = document.getElementById('content');
    content.innerHTML = reader.result;
    expectedContent = reader.result;
    readEntryByUrl(entry.toURL());
  };
  reader.onerror = errorCallback;
  entry.file(function(file) {
    reader.readAsText(file);
  });
};

function onTabLoaded() {
  var entries = chrome.extension.getBackgroundPage().getLastFileEntries();
  if (!entries || entries.length != 1 || !entries[0]) {
    chrome.extension.sendRequest(
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

window.addEventListener("load", onTabLoaded, false);
