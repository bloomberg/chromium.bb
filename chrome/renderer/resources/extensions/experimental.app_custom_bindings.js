// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the experimental.app API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var fileSystemHelpers = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;

chromeHidden.registerCustomHook('experimental.app', function(bindingsAPI) {
  chrome.experimental.app.onLaunched.dispatch =
      function(launchData, fileSystemId, baseName) {
    if (launchData) {
      var event = this;
      var fs = GetIsolatedFileSystem(fileSystemId);
      try {
        fs.root.getFile(baseName, {}, function(fileEntry) {
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
    } else {
      chrome.Event.prototype.dispatch.call(this);
    }
  };
});
