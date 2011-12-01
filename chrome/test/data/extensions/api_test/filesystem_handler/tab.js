// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a file intent handler and does the following during the test:

1. Its background page first registers content hander.
2. When content handler callback is invoked, opens tab.html page and passes
   file url via hash ref.
3. Tries to resolve target file url and reads its content.
4. Send file content to file browser extension.
*/

// The ID of the extension we want to talk to.
var fileBrowserExtensionId = "ddammdhioacbehjngdmkjcjbnfginlla";

// Passed file entry url.
var entryUrl = null;
// Expected file content.
var expectedContent = null;

function errorCallback(e) {
  var msg = '';
  if (!e.code) {
    msg = e.message;
  } else {
    switch (e.code) {
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
  chrome.extension.sendRequest(fileBrowserExtensionId,
                               {fileContent: null,
                                error: "Handler file error: " + msg},
    function(response) {});
}

function onGotEntryByUrl(entry) {
  console.log('Got entry by URL: ' + entry.toURL());
  var reader = new FileReader();
  reader.onloadend = function(e) {
      if (reader.result != expectedContent) {
        chrome.extension.sendRequest(
            fileBrowserExtensionId,
            {fileContent: null, error: "File content does not match."},
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

function readEntryByUrl() {
  window.webkitResolveLocalFileSystemURL(entryUrl, onGotEntryByUrl,
                                         errorCallback);
}

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
  var reader = new FileReader();
  entryUrl = entry.toURL();
  reader.onloadend = function(e) {
    var content = document.getElementById('content');
    content.innerHTML = reader.result;
    expectedContent = reader.result;
    readEntryByUrl();
  };
  reader.onerror = errorCallback;
  entry.file(function(file) {
    reader.readAsText(file);
  });
}

window.addEventListener("load", onTabLoaded, false);
