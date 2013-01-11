// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ID of the extension we want to talk to.
var fileBrowserExtensionId = "ddammdhioacbehjngdmkjcjbnfginlla";

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
  var reader = new FileReader();
  reader.onloadend = function(e) {
    // Send data back to the file browser extension
    chrome.runtime.sendMessage(
        fileBrowserExtensionId,
        {fileContent: reader.result, error: null},
        function(response) {});
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

readEntryByUrl(localStorage.entryURL);
