// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
This extension is a file intent handler and does the following during the test:

1. It first registers content hander.
2. When content handler callback is invoked, opens tab.html page and passes
   file url via hash ref.
3. Tries to resolve target file url and reads its content.
4. Tries to append file with the read content.
5. Send the read file content to file browser extension.
*/

// The ID of the extension we want to talk to.
var fileBrowserExtensionId = 'ddammdhioacbehjngdmkjcjbnfginlla';

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

function onSuccess(text) {
  chrome.runtime.sendMessage(
      fileBrowserExtensionId,
      {fileContent: text, error: null},
      function(response) {});
};

function writeToFile(entry, text) {
  console.log('create writer');
  entry.createWriter(function(writer) {
    writer.onerror = function(e) {
      errorCallback(writer.error);
    };
    writer.onwrite = onSuccess.bind(this, text);

    var blob = new Blob([text + text], {type: 'text/plain'});
    writer.write(blob);
  });
};

function runFileSystemHandlerTest(entries) {
  if (!entries || entries.length != 1 || !entries[0]) {
    chrome.runtime.sendMessage(
        fileBrowserExtensionId,
        {fileContent: null, error: 'Invalid file entries.'},
        function(response) {});
    return;
  }
  var entry = entries[0];
  var reader = new FileReader();
  reader.onloadend = function(e) {
    writeToFile(entry, reader.result);
  };
  reader.onerror = function(e) {
    errorCallback({message: 'Unable to read file.'});
  };
  entry.file(function(file) {
    reader.readAsText(file);
  },
  errorCallback);
}

function executeListener(id, details) {
  if (id != 'TestAction_aBc' && id != 'TestAction_def' && id != '123Action') {
    errorCallback({message: 'Unexpected action id: ' + id});
    return;
  }
  var file_entries = details.entries;
  if (!file_entries || file_entries.length != 1) {
    errorCallback({message: 'Unexpected file url list'});
    return;
  }
  chrome.tabs.get(details.tab_id, function(tab) {
    if (tab.title != 'file browser component test') {
      errorCallback({message: 'Unexpected tab title: ' + tab.title});
      return;
    }
    runFileSystemHandlerTest(file_entries);
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
