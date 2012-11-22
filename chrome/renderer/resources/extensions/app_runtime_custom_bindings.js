// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the chrome.app.runtime API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var fileSystemHelpers = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;
var appNatives = requireNative('app_runtime');
var DeserializeString = appNatives.DeserializeString;
var SerializeToString = appNatives.SerializeToString;
var CreateBlob = appNatives.CreateBlob;

chromeHidden.Event.registerArgumentMassager('app.runtime.onLaunched',
    function(args, dispatch) {
  var launchData = args[0];
  var intentData = args[1];
  var intentId = args[2];

  if (launchData && typeof launchData.id !== 'undefined') {
    // new-style dispatch.
    var items = []
    var numItems = launchData.items.length;
    var itemLoaded = function(err, item) {
      if (err) {
        console.error('Error getting fileEntry, code: ' + err.code);
      } else {
        items.push(item);
      }
      if (--numItems === 0) {
        if (items.length === 0) {
          dispatch([]);
        } else {
          var data = { id: launchData.id, items: items };
          // TODO(benwells): remove once we no longer support intents.
          data.intent = {
            action: "http://webintents.org/view",
            type: "chrome-extension://fileentry",
            data: items[0].entry,
            postResult: function() {},
            postFailure: function() {}
          };
          dispatch([data]);
        }
      }
    };
    launchData.items.forEach(function(item) {
      var fs = GetIsolatedFileSystem(item.fileSystemId);
      fs.root.getFile(item.baseName, {}, function(fileEntry) {
        itemLoaded(null, { entry: fileEntry, type: item.mimeType });
      }, function(fileError) {
        itemLoaded(fileError);
      });
    });
  } else {
    if (launchData) {
      if (intentId) {
        var fn = function(success, data) {
          chrome.app.runtime.postIntentResponse({
            'intentId': intentId,
            'success': success,
            'data': SerializeToString(data)
          });
        };
        launchData.intent.postResult = fn.bind(undefined, true);
        launchData.intent.postFailure = fn.bind(undefined, false);
      } else {
        launchData.intent.postResult = function() {};
        launchData.intent.postFailure = function() {};
      }
    }

    if (launchData && intentData) {
      switch(intentData.format) {
        case('fileEntry'):
          var fs = GetIsolatedFileSystem(intentData.fileSystemId);
          try {
            fs.root.getFile(intentData.baseName, {}, function(fileEntry) {
              launchData.intent.data = fileEntry;
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
        case('filesystem'):
          launchData.intent.data = GetIsolatedFileSystem(
              intentData.fileSystemId, intentData.baseName);
          launchData.intent.postResult = function() {};
          launchData.intent.postFailure = function() {};
          dispatch([launchData]);
          break;
        case('serialized'):
          var deserializedData = DeserializeString(intentData.data);
          launchData.intent.data = deserializedData;
          dispatch([launchData]);
          break;
        case('blob'):
          var blobData = CreateBlob(intentData.blobFilePath,
                                    intentData.blobLength);
          launchData.intent.data = blobData;
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
  }
});
