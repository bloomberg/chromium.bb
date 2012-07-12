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

chromeHidden.Event.registerArgumentMassager('experimental.app.onLaunched',
    function(args, dispatch) {
  var launchData = args[0];
  var intentData = args[1];

  if (launchData && intentData) {
    switch(intentData.format) {
      case('fileEntry'):
        var fs = GetIsolatedFileSystem(intentData.fileSystemId);
        try {
          fs.root.getFile(intentData.baseName, {}, function(fileEntry) {
            launchData.intent.data = fileEntry;
            launchData.intent.postResult = function() {};
            launchData.intent.postFailure = function() {};
            dispatch([launchData]);
          }, function(fileError) {
            console.error('Error getting fileEntry, code: ' + fileError.code);
            dispatch([]);
          });
        } catch (e) {
          console.error('Error in event handler for onLaunched: ' + e.stack);
          dispatch([]);
        }
        break;
      case('serialized'):
        var deserializedData = DeserializeString(intentData.data);
        launchData.intent.data = deserializedData;
        launchData.intent.postResult = function() {};
        launchData.intent.postFailure = function() {};
        dispatch([launchData]);
        break;
      case('blob'):
        var blobData = CreateBlob(intentData.blobFilePath,
                                  intentData.blobLength);
        launchData.intent.data = blobData;
        launchData.intent.postResult = function() {};
        launchData.intent.postFailure = function() {};
        dispatch([launchData]);
        break;
      default:
        console.error('Unexpected launch data format');
        dispatch([]);
    }
  } else if (launchData) {
    dispatch([launchData]);
  } else {
    dispatch([]);
  }
});
