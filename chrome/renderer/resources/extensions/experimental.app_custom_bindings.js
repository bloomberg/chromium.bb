// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the experimental.app API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var fileSystemHelpers = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;
var appNatives = requireNative('experimental_app');
var DeserializeString = appNatives.DeserializeString;
var CreateBlob = appNatives.CreateBlob;

chromeHidden.registerCustomHook('experimental.app', function(bindingsAPI) {
  chrome.experimental.app.onLaunched.dispatch =
      function(launchData, intentData) {
    // TODO(benwells): Remove this logging.
    console.log("In onLaunched dispatch hook.");
    if (launchData && intentData) {
      switch(intentData.format) {
        case('fileEntry'):
          var event = this;
          var fs = GetIsolatedFileSystem(intentData.fileSystemId);
          try {
            fs.root.getFile(intentData.baseName, {}, function(fileEntry) {
              launchData.intent.data = fileEntry;
              launchData.intent.postResult = function() {};
              launchData.intent.postFailure = function() {};
              chrome.Event.prototype.dispatch.call(event, launchData);
            }, function(fileError) {
              console.error('Error getting fileEntry, code: ' + fileError.code);
              chrome.Event.prototype.dispatch.call(event);
            });
          } catch (e) {
            console.error('Error in event handler for onLaunched: ' + e.stack);
            chrome.Event.prototype.dispatch.call(event);
          }
          break;
        case('serialized'):
          var deserializedData = DeserializeString(intentData.data);
          launchData.intent.data = deserializedData;
          launchData.intent.postResult = function() {};
          launchData.intent.postFailure = function() {};
          chrome.Event.prototype.dispatch.call(this, launchData);
          break;
        case('blob'):
          var blobData = CreateBlob(intentData.blobFilePath,
                                    intentData.blobLength);
          launchData.intent.data = blobData;
          launchData.intent.postResult = function() {};
          launchData.intent.postFailure = function() {};
          chrome.Event.prototype.dispatch.call(this, launchData);
          break;
        default:
          console.error('Unexpected launch data format');
          chrome.Event.prototype.dispatch.call(this);
      }
    } else if (launchData) {
      chrome.Event.prototype.dispatch.call(this, launchData);
    } else {
      chrome.Event.prototype.dispatch.call(this);
    }
  };
});
